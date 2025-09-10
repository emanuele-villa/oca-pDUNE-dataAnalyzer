// Aggregates multiple runs into per-beam-condition ROOT files using parameters/beam_settings.dat
#include "TFile.h"
#include "TTree.h"
#include "TNamed.h"
#include "TDirectory.h"
// #include "TEntryList.h" // unused legacy include removed
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <climits>

// Helper: parse YYYYMMDD_HHMMSS from SCD filename and return time_t in UTC
static bool parseUtcFromRunName(const std::string& base, std::tm& outUtc) {
    // Expect pattern: SCD_RUN<5d>_BEAM_YYYYMMDD_HHMMSS
    size_t pos = base.find("_BEAM_");
    if (pos == std::string::npos) return false;
    std::string ts = base.substr(pos + 6); // YYYYMMDD_HHMMSS
    if (ts.size() < 15) return false;
    std::string ymd = ts.substr(0, 8);
    std::string hms = ts.substr(9, 6);
    std::tm t{}; t.tm_isdst = 0;
    t.tm_year = std::stoi(ymd.substr(0,4)) - 1900;
    t.tm_mon  = std::stoi(ymd.substr(4,2)) - 1;
    t.tm_mday = std::stoi(ymd.substr(6,2));
    t.tm_hour = std::stoi(hms.substr(0,2));
    t.tm_min  = std::stoi(hms.substr(2,2));
    t.tm_sec  = std::stoi(hms.substr(4,2));
    outUtc = t;
    return true;
}

// Parse beam_settings.dat: simplistic parser ignoring comments and header separators
struct BeamSetting {
    std::tm cest{}; // local CEST time (UTC+2 assumed)
    std::string energy;
    std::string target;
    std::string cherenkov;
    std::string collimator;
    std::string details;
    std::string label; // e.g. "Beam at +1 GeV, ..."
};

static std::vector<BeamSetting> readBeamSettings(const std::string& path) {
    std::vector<BeamSetting> out;
    std::ifstream in(path);
    if (!in.is_open()) return out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.find("---") != std::string::npos) continue;
    // Skip header line if present (contains column titles like 'Energy')
    if (line.find("Energy") != std::string::npos && line.find("Date") != std::string::npos) continue;
        std::istringstream ss(line);
        std::string date, time, energy, target, cherenkov, collimator, details;
        // Expect pipe-separated columns with spaces around pipes
        // Normalize by removing spaces around pipes
        std::string norm; norm.reserve(line.size());
        for (char c : line) {
            if (c=='\t') c=' ';
            norm.push_back(c);
        }
        // Split by '|'
        std::vector<std::string> cols; cols.reserve(7);
        std::stringstream ss2(norm);
        std::string seg;
        while (std::getline(ss2, seg, '|')) {
            // trim
            size_t b = seg.find_first_not_of(" \t");
            size_t e = seg.find_last_not_of(" \t");
            if (b==std::string::npos) cols.emplace_back("");
            else cols.emplace_back(seg.substr(b, e-b+1));
        }
        if (cols.size() < 7) continue;
        BeamSetting bs;
        // Date YYYY-MM-DD, Time HH:MM
        if (cols[0].size()>=10 && cols[1].size()>=5) {
            std::tm t{}; t.tm_isdst = -1; // let system decide
            t.tm_year = std::stoi(cols[0].substr(0,4)) - 1900;
            t.tm_mon  = std::stoi(cols[0].substr(5,2)) - 1;
            t.tm_mday = std::stoi(cols[0].substr(8,2));
            t.tm_hour = std::stoi(cols[1].substr(0,2));
            t.tm_min  = std::stoi(cols[1].substr(3,2));
            t.tm_sec  = 0;
            bs.cest = t;
        }
        bs.energy    = cols[2];
        bs.target    = cols[3];
        bs.cherenkov = cols[4];
        bs.collimator= cols[5];
        bs.details   = cols[6];
        bs.label = std::string("Beam at ") + bs.energy + " GeV" + (bs.target!="-"? ", Target: "+bs.target:"")
                   + (bs.cherenkov!="-"? ", Cherenkov: "+bs.cherenkov:"")
                   + (bs.collimator!="-"? ", Collimator: "+bs.collimator:"")
                   + (bs.details!="-"? ", "+bs.details:"");
        out.emplace_back(std::move(bs));
    }
    // Sort by time ascending
    std::sort(out.begin(), out.end(), [](const BeamSetting& a, const BeamSetting& b){
        return std::mktime(const_cast<std::tm*>(&a.cest)) < std::mktime(const_cast<std::tm*>(&b.cest));
    });
    return out;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: beamAggregator <beam_settings.dat> <out_dir> <root1> [root2 ...]" << std::endl;
        return 1;
    }
    std::string settingsPath = argv[1];
    std::string outDir = argv[2];
    std::vector<std::string> inputs; for (int i=3;i<argc;i++) inputs.emplace_back(argv[i]);

    std::cout << "[beamAggregator] Reading beam settings from: " << settingsPath << std::endl;
    auto settings = readBeamSettings(settingsPath);
    if (settings.empty()) {
        std::cerr << "No beam settings parsed from " << settingsPath << std::endl;
        return 1;
    }
    std::cout << "[beamAggregator] Parsed settings: " << settings.size() << std::endl;

    std::filesystem::create_directories(outDir);

    // For each input ROOT, determine UTC base from filename; iterate event_info timestamps to compute absolute times (UTC)
    struct EVT { long long absTicks; int srcIdx; long long ts; int entry; };
    const double TICK_PERIOD_SEC = 20e-9; // 20 ns internal ticks
    std::vector<EVT> all;
    std::vector<TFile*> files; files.reserve(inputs.size());
    std::vector<TTree*> infos; infos.reserve(inputs.size());
    std::vector<long long> baseTicks; baseTicks.reserve(inputs.size());
    std::cout << "[beamAggregator] Candidate inputs: " << inputs.size() << std::endl;
    for (size_t i=0;i<inputs.size();++i) {
        const std::string& path = inputs[i];
        std::cout << "[beamAggregator] Open input " << i << ": " << path << std::endl;
        TFile* f = TFile::Open(path.c_str(), "READ");
        if (!f || f->IsZombie()) { std::cerr << "[beamAggregator] WARN: Cannot open " << path << ", skipping." << std::endl; if (f) { f->Close(); delete f; } continue; }
        TTree* info = (TTree*)f->Get("event_info");
        if (!info) { std::cerr << "[beamAggregator] WARN: Missing event_info in " << path << ", skipping." << std::endl; f->Close(); delete f; continue; }
        if (!info->GetBranch("timestamp")) { std::cerr << "[beamAggregator] WARN: event_info has no 'timestamp' in " << path << ", skipping." << std::endl; f->Close(); delete f; continue; }
        // parse UTC base from filename
        std::string base = path.substr(path.find_last_of("/\\")+1);
        if (base.size()>13 && base.rfind(".root")!=std::string::npos) base = base.substr(0, base.size()-5);
        std::tm tUTC{};
        if (!parseUtcFromRunName(base, tUTC)) { std::cerr << "[beamAggregator] WARN: Cannot parse UTC from " << base << ", skipping." << std::endl; f->Close(); delete f; continue; }
        time_t epoch = timegm(&tUTC); // UTC epoch
        long long epochTicks = (long long)((double)epoch / TICK_PERIOD_SEC);

        // accept this input
        int srcIdx = (int)files.size();
        files.push_back(f);
        infos.push_back(info);
        baseTicks.push_back(epochTicks);

        // iterate timestamps
    Long64_t ts=0; info->SetBranchAddress("timestamp", &ts);
    int n = info->GetEntries();
    std::cout << "[beamAggregator] event_info entries for input " << srcIdx << ": " << n << std::endl;
    for (int e=0;e<n;++e) { info->GetEntry(e); all.push_back({ baseTicks.back() + (long long)ts, srcIdx, (long long)ts, e }); }
    // IMPORTANT: clear branch addresses so we don't leave dangling pointers to stack variable 'ts'
    info->ResetBranchAddresses();
    }
    std::cout << "[beamAggregator] Valid inputs: " << files.size() << std::endl;
    // sort all events by absolute ticks
    std::cout << "[beamAggregator] Total events across inputs: " << all.size() << std::endl;
    std::sort(all.begin(), all.end(), [](const EVT&a,const EVT&b){return a.absTicks<b.absTicks;});

        // Convert beam setting times to CEST seconds since epoch (uniform domain)
        struct SettingWindow { double startCEST; std::string energyRaw; std::string label; };
        std::vector<SettingWindow> windows; windows.reserve(settings.size());
        for (auto &s : settings) {
            std::tm tmC = s.cest; // CEST expressed as given
            time_t utcLike = timegm(&tmC); // interprets given fields as UTC -> we later shift +7200
            double cestSec = (double)utcLike + 7200.0;
            windows.push_back({cestSec, s.energy, s.label});
        }
        std::sort(windows.begin(), windows.end(), [](auto&a,auto&b){return a.startCEST < b.startCEST;});

        // Build event list with CEST absolute times (seconds)
        struct AggEvent { double cestTime; int src; Long64_t entry; };
        std::vector<AggEvent> events; events.reserve(all.size());
        for (auto &ev : all) {
            // absTicks currently based on epochTicks + internal ticks; convert ticks->seconds then +7200 shift to CEST
            double utcSec = ev.absTicks * TICK_PERIOD_SEC; // since absTicks = epochTicks + internal ticks
            double cestSec = utcSec + 7200.0;
            events.push_back({cestSec, ev.srcIdx, (Long64_t)ev.entry});
        }
        std::sort(events.begin(), events.end(), [](const AggEvent&a,const AggEvent&b){return a.cestTime < b.cestTime;});
        if (events.empty()) { std::cout << "[beamAggregator] No events found. Exiting." << std::endl; for (auto* f: files){ if(f){f->Close(); delete f;}} return 0; }

        double firstEvt = events.front().cestTime;
        double lastEvt  = events.back().cestTime;
        std::cout << "[beamAggregator] Event CEST span: " << (lastEvt-firstEvt) << " s" << std::endl;

        size_t eIndex = 0;
        for (size_t wi=0; wi<windows.size(); ++wi) {
            double start = windows[wi].startCEST;
            const double namingStart = start; // preserve original (unclamped) start for output filename
            double end   = (wi+1<windows.size()? windows[wi+1].startCEST : (lastEvt + 1.0));
            if (end <= start) continue; // malformed window ordering
            if (end < firstEvt) continue; // window entirely before data
            bool clamped = false;
            if (start < firstEvt) { start = firstEvt; clamped = true; }
            // advance eIndex to first event >= start
            while (eIndex < events.size() && events[eIndex].cestTime < start) ++eIndex;
            size_t beginIdx = eIndex;
            while (eIndex < events.size() && events[eIndex].cestTime < end) ++eIndex;
            size_t endIdx = eIndex; // one past last
            size_t count = (endIdx>beginIdx)? (endIdx-beginIdx) : 0;
            std::cout << "[beamAggregator] Window " << wi << " startCEST=" << start << " endCEST=" << end << " events=" << count << std::endl;
            if (count == 0) continue;
            // Build energy token
            std::string eraw = windows[wi].energyRaw;
            // Trim spaces
            auto trim=[&](std::string &s){size_t b=s.find_first_not_of(" \t"); size_t e=s.find_last_not_of(" \t"); if(b==std::string::npos){s="";return;} s=s.substr(b,e-b+1);}; trim(eraw);
            char sign='+'; if(!eraw.empty() && (eraw[0]=='+'||eraw[0]=='-')) { sign=eraw[0]; eraw=eraw.substr(1); }
            // Extract numeric part
            std::string num; for(char c: eraw){ if((c>='0'&&c<='9')||c=='.') num.push_back(c); else break; }
            if (num.empty()) { std::cout << "[beamAggregator] Skipping window "<<wi<<" due to non-numeric energy '"<<windows[wi].energyRaw<<"'\n"; continue; }
            // Replace '.' with 'p' in numeric part to make filename parser-friendly (e.g. 1.5 -> 1p5)
            std::string numSanitized = num; for (char &c : numSanitized) if (c=='.') c='p';
            std::string energyToken = std::string(sign=='-'?"minus":"plus") + numSanitized + "GeV";
            // Format start time (use CEST). Convert to UTC for gmtime by subtracting 7200 then add back logically in formatting
            // Use original window start for naming (even if clamped for event selection) to reflect scheduled beam setting time
            time_t baseUtc = (time_t)(namingStart - 7200.0);
            std::tm *gmt = gmtime(&baseUtc);
            // Reconstruct CEST components by +2h with day rollover
            int year = gmt->tm_year + 1900; int mon = gmt->tm_mon+1; int mday=gmt->tm_mday; int hour=gmt->tm_hour + 2; int min=gmt->tm_min; int sec=gmt->tm_sec;
            if (hour>=24){ hour-=24; // simple rollover (ignore month/year edges for simplicity; acceptable because +2h can't skip more than one day)
                // adjust day (not perfect for month end but rarely window starts exactly at 23/24 UTC boundary). For correctness implement month/day roll.
                static int mdays[12]={31,28,31,30,31,30,31,31,30,31,30,31}; int days=mdays[mon-1]; if(mon==2 && ((year%4==0&&year%100!=0)||year%400==0)) days=29; if(++mday>days){ mday=1; if(++mon>12){mon=1; ++year;} }
            }
            char timestr[32]; snprintf(timestr,sizeof(timestr),"%04d%02d%02d_%02d%02d%02d",year,mon,mday,hour,min,sec);
            std::string outName = std::string(timestr) + "_" + energyToken + ".root";
            std::string outPath = outDir + "/" + outName;
            std::cout << "[beamAggregator] Creating output: " << outPath << std::endl;
            TFile* fout = TFile::Open(outPath.c_str(), "RECREATE");
            if (!fout || fout->IsZombie()) { std::cerr << "Cannot create " << outPath << std::endl; if(fout) {delete fout;} continue; }
            TNamed title("beam_label", windows[wi].label.c_str()); title.Write();
            // event_info
            TTree* outInfo = infos[0]->CloneTree(0); outInfo->SetDirectory(fout);
            int lastSrc=-1; Long64_t filled=0;
            for (size_t j=beginIdx; j<endIdx; ++j) {
                auto &ae = events[j];
                TTree* src = infos[ae.src];
                if (ae.src != lastSrc) { outInfo->CopyAddresses(src); lastSrc = ae.src; }
                src->GetEntry(ae.entry);
                outInfo->Fill(); ++filled;
            }
            outInfo->Write("event_info");
            // Detector trees
            const char* detNamesNew[4] = {"detector_0","detector_1","detector_2","detector_3"};
            const char* detNamesLegacy[4] = {"raw_events","raw_events_B","raw_events_C","raw_events_D"};
            for (int di=0; di<4; ++di) {
                TTree* in0 = (TTree*)files[0]->Get(detNamesNew[di]);
                const char* useName = detNamesNew[di];
                if (!in0) { in0 = (TTree*)files[0]->Get(detNamesLegacy[di]); useName = detNamesLegacy[di]; }
                if (!in0) continue;
                TTree* outDet = in0->CloneTree(0); outDet->SetName(useName); outDet->SetDirectory(fout);
                int lastDSrc=-1; Long64_t dFilled=0;
                for (size_t j=beginIdx; j<endIdx; ++j) {
                    auto &ae = events[j];
                    TTree* src = (TTree*)files[ae.src]->Get(useName);
                    if (!src) continue;
                    if (ae.src != lastDSrc) { outDet->CopyAddresses(src); lastDSrc = ae.src; }
                    src->GetEntry(ae.entry);
                    outDet->Fill(); ++dFilled;
                }
                outDet->Write(useName);
                delete outDet;
            }
            fout->Write(); fout->Close(); delete fout;
            std::cout << "[beamAggregator] Wrote " << outPath << " with events=" << (endIdx-beginIdx) << std::endl;
        }

    for (auto* f: files) { if (f) { f->Close(); delete f; } }
    return 0;
}
