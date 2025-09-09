// Aggregates multiple runs into per-beam-condition ROOT files using parameters/beam_settings.dat
#include "TFile.h"
#include "TTree.h"
#include "TNamed.h"
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

    auto settings = readBeamSettings(settingsPath);
    if (settings.empty()) {
        std::cerr << "No beam settings parsed from " << settingsPath << std::endl;
        return 1;
    }

    std::filesystem::create_directories(outDir);

    // For each input ROOT, determine UTC base from filename; iterate event_info timestamps to compute absolute times (UTC)
    struct EVT { long long absTicks; int srcIdx; long long ts; int entry; };
    const double TICK_PERIOD_SEC = 20e-9; // 20 ns internal ticks
    std::vector<EVT> all;
    std::vector<TFile*> files; files.reserve(inputs.size());
    std::vector<TTree*> infos; infos.reserve(inputs.size());
    std::vector<long long> baseTicks; baseTicks.reserve(inputs.size());
    for (size_t i=0;i<inputs.size();++i) {
        TFile* f = TFile::Open(inputs[i].c_str(), "READ");
        if (!f || f->IsZombie()) { std::cerr << "Cannot open " << inputs[i] << std::endl; return 2; }
        files.push_back(f);
        TTree* info = (TTree*)f->Get("event_info");
        if (!info) { std::cerr << "Missing event_info in " << inputs[i] << std::endl; return 2; }
        infos.push_back(info);
        // parse UTC base from filename
        std::string base = inputs[i].substr(inputs[i].find_last_of("/\\")+1);
        if (base.size()>13 && base.rfind(".root")!=std::string::npos) base = base.substr(0, base.size()-5);
        std::tm tUTC{};
        if (!parseUtcFromRunName(base, tUTC)) { std::cerr << "Cannot parse UTC from " << base << std::endl; return 2; }
        time_t epoch = timegm(&tUTC); // UTC epoch
        long long epochTicks = (long long)(epoch / TICK_PERIOD_SEC);
        baseTicks.push_back(epochTicks);
        // iterate timestamps
        ULong64_t ts=0; info->SetBranchAddress("timestamp", &ts);
        int n = info->GetEntries();
        for (int e=0;e<n;++e) { info->GetEntry(e); all.push_back({ baseTicks.back() + (long long)ts, (int)i, (long long)ts, e }); }
    }
    // sort all events by absolute ticks
    std::sort(all.begin(), all.end(), [](const EVT&a,const EVT&b){return a.absTicks<b.absTicks;});

    // Build CEST times for settings: UTC = CEST-2h (approx; ignoring DST transitions)
    std::vector<long long> settingUtcTicks; settingUtcTicks.reserve(settings.size());
    for (auto& s: settings) {
        std::tm u = s.cest; u.tm_hour -= 2; time_t epoch = timegm(&u);
        long long ticks = (long long)(epoch / TICK_PERIOD_SEC);
        settingUtcTicks.push_back(ticks);
    }

    // For each interval between settings[i] and settings[i+1), collect events and write to new ROOT
    for (size_t si=0; si<settings.size(); ++si) {
        long long startT = settingUtcTicks[si];
        long long endT = (si+1<settings.size()? settingUtcTicks[si+1] : LLONG_MAX);
        // pick events in [startT, endT)
        std::vector<const EVT*> window;
        for (auto& ev: all) if (ev.absTicks >= startT && ev.absTicks < endT) window.push_back(&ev);
        if (window.empty()) continue;
        // Output file name based on start time and energy
        char buf[32];
        time_t startSec = (time_t)(startT * TICK_PERIOD_SEC);
        std::tm* pt = gmtime(&startSec);
        strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", pt);
        std::string cleanEnergy = settings[si].energy; // already includes sign and value
        std::string outName = std::string(buf) + "_" + (cleanEnergy.size()? (cleanEnergy[0]=='+'?"plus":"minus"):"beam") + (cleanEnergy.size()? cleanEnergy.substr(cleanEnergy[0]=='+'||cleanEnergy[0]=='-'?1:0):"") + "GeV.root";
        std::string outPath = outDir + "/" + outName;
        // Create output file and copy selected entries from each input; copy event_info and detector trees
        TFile* fout = TFile::Open(outPath.c_str(), "RECREATE");
        if (!fout || fout->IsZombie()) { std::cerr << "Cannot create " << outPath << std::endl; continue; }
        // Write a TNamed with the beam label for downstream use
        TNamed title("beam_label", settings[si].label.c_str());
        title.Write();
        // Create new event_info tree with selected entries in chronological order
        TTree* outInfo = infos[0]->CloneTree(0);
        for (auto* pev : window) {
            // ensure output branches point to the same buffers as the current source before reading
            outInfo->CopyAddresses(infos[pev->srcIdx]);
            infos[pev->srcIdx]->GetEntry(pev->entry);
            outInfo->Fill();
        }
        outInfo->Write("event_info");
        // For detector trees, mirror the selection per source by entry ordering
        const char* detNamesNew[4] = {"detector_0","detector_1","detector_2","detector_3"};
        const char* detNamesLegacy[4] = {"raw_events","raw_events_B","raw_events_C","raw_events_D"};
        for (int di=0; di<4; ++di) {
            // For simplicity, try new name then legacy from source 0; CloneTree structure from first non-null
            TTree* in0 = (TTree*)files[0]->Get(detNamesNew[di]);
            const char* useName = detNamesNew[di];
            if (!in0) { in0 = (TTree*)files[0]->Get(detNamesLegacy[di]); useName = detNamesLegacy[di]; }
            if (!in0) continue;
            TTree* outDet = in0->CloneTree(0);
            outDet->SetName(useName);
            for (auto* pev : window) {
                TTree* srcDet = (TTree*)files[pev->srcIdx]->Get(useName);
                if (!srcDet) continue;
                outDet->CopyAddresses(srcDet);
                srcDet->GetEntry(pev->entry);
                outDet->Fill();
            }
            outDet->Write(useName);
        }
        fout->Write();
        fout->Close();
        std::cout << "Wrote " << outPath << " with " << window.size() << " events" << std::endl;
    }

    for (auto* f: files) { f->Close(); }
    return 0;
}
