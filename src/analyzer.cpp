// analyzer.cpp
// Reads *_clusters.root, makes plots and PDF report
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TPad.h"
#include "TGraph.h"
#include "TDatime.h"
#include "TString.h"
#include <vector>
#include <string>
#include <iostream>

static std::string inferPdfPath(const std::string &clustersPath) {
    std::string pdf = clustersPath;
    size_t pos = pdf.rfind(".root");
    if (pos != std::string::npos) pdf.replace(pos, 5, ".pdf"); else pdf += ".pdf";
    return pdf;
}

int main(int argc, char** argv) {
    // Args: -i input_clusters.root [-o output.pdf] [-r input_converted.root]
    std::string clustersPath, pdfPath, convertedRootPath;
    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        if ((a=="-i"||a=="--input") && i+1<argc) clustersPath = argv[++i];
        else if ((a=="-o"||a=="--output") && i+1<argc) pdfPath = argv[++i];
        else if ((a=="-r"||a=="--root-file") && i+1<argc) convertedRootPath = argv[++i];
        else if (a=="-h"||a=="--help") { std::cout << "Usage: analyzer -i file_clusters.root [-o report.pdf] [-r file_converted.root]" << std::endl; return 0; }
    }
    if (clustersPath.empty()) { std::cerr << "Error: missing -i input clusters file" << std::endl; return 1; }
    if (pdfPath.empty()) pdfPath = inferPdfPath(clustersPath);

    TFile* fin = TFile::Open(clustersPath.c_str(), "READ");
    if (!fin || !fin->IsOpen()) { std::cerr << "Failed to open " << clustersPath << std::endl; return 2; }
    TTree* clusters = (TTree*)fin->Get("clusters");
    if (!clusters) { std::cerr << "Missing clusters TTree" << std::endl; return 3; }

    // Pull basic histograms if present
    TH1F* h_hits = (TH1F*)fin->Get("h_hitsPerEvent");
    TH1F* h_cpe[4] = {nullptr,nullptr,nullptr,nullptr};
    for (int d=0; d<4; ++d) h_cpe[d] = (TH1F*)fin->Get(Form("h_clustersPerEvent_D%d", d));

    // First page canvas
    TCanvas c1("c_summary","Summary",1000,700);
    c1.cd();
    TLatex lat; lat.SetNDC(); lat.SetTextSize(0.035);
    // Header with date/time in CEST (we display label only)
    TDatime now; lat.DrawLatex(0.10, 0.95, Form("Report generated: %04d-%02d-%02d %02d:%02d:%02d CEST", now.GetYear(), now.GetMonth(), now.GetDay(), now.GetHour(), now.GetMinute(), now.GetSecond()));
    lat.SetTextSize(0.045); lat.DrawLatex(0.10, 0.90, "Beam monitor analysis summary");
    lat.SetTextSize(0.032);
    double y=0.82;
    if (h_hits) {
        lat.DrawLatex(0.10, y, Form("Events with hits: %lld", (long long)h_hits->GetEntries())); y -= 0.04;
        lat.DrawLatex(0.10, y, Form("Mean hits per event: %.2f", h_hits->GetMean())); y -= 0.04;
    }
    for (int d=0; d<4; ++d) {
        if (h_cpe[d]) { lat.DrawLatex(0.10, y, Form("D%d: clusters/event mean=%.3f", d, h_cpe[d]->GetMean())); y -= 0.035; }
    }
    c1.Print((pdfPath+"(").c_str());

    // Plots: hits per event
    if (h_hits) { TCanvas c("c_hits","Hits per event",800,600); c.cd(); h_hits->SetTitle("Hits per event;Hits;Events"); h_hits->Draw(); c.Print(pdfPath.c_str()); }
    // Clusters per event per detector (linear scale)
    for (int d=0; d<4; ++d) {
        if (!h_cpe[d]) continue; TCanvas c(Form("c_cpe_%d",d),Form("Clusters/event D%d",d),800,600); c.cd(); h_cpe[d]->SetTitle(Form("Clusters per event - D%d;Clusters;Events", d)); h_cpe[d]->Draw(); c.Print(pdfPath.c_str());
    }

    // Timestamp plots from converted root if provided
    if (!convertedRootPath.empty()) {
        TFile* fr = TFile::Open(convertedRootPath.c_str(), "READ");
        if (fr && fr->IsOpen()) {
            TTree* info = (TTree*)fr->Get("event_info");
            if (info && info->GetBranch("timestamp")) {
                const double TICK=20e-9, EXT_TICK=64e-9;
                Long64_t ts=0, ext=0; Long64_t trig=0; bool hasTrig = info->GetBranch("trigger_number") && info->SetBranchAddress("trigger_number", &trig) >= 0; 
                info->SetBranchAddress("timestamp", &ts);
                if (info->GetBranch("ext_timestamp")) info->SetBranchAddress("ext_timestamp", &ext);
                Long64_t n = info->GetEntries();
                // Try to parse start datetime from converted file name for titles (YYYYMMDD_HHMMSS tokens)
                auto makeSinceLabel = [&](){
                    std::string base = convertedRootPath;
                    size_t slash = base.find_last_of("/\\"); if (slash!=std::string::npos) base = base.substr(slash+1);
                    size_t posTime = base.rfind('_');
                    auto isDigits=[&](const std::string&s){return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);} ;
                    if (posTime!=std::string::npos && posTime+1<base.size()) {
                        std::string timeTok = base.substr(posTime+1); // HHMMSS.root or HHMMSS_converted.root
                        // strip extension and suffixes
                        size_t dot = timeTok.find('.'); if (dot!=std::string::npos) timeTok = timeTok.substr(0,dot);
                        size_t under = base.rfind('_', posTime-1);
                        if (under!=std::string::npos && under+1<posTime) {
                            std::string dateTok = base.substr(under+1, posTime-under-1); // YYYYMMDD
                            if (dateTok.size()==8 && (timeTok.size()==6 || timeTok.size()==4) && isDigits(dateTok) && isDigits(timeTok)) {
                                std::string lab = Form("since %4.4s-%2.2s-%2.2s %2.2s:%2.2s CEST",
                                    dateTok.c_str(), dateTok.c_str()+4, dateTok.c_str()+6,
                                    timeTok.c_str(), timeTok.c_str()+2);
                                return lab;
                            }
                        }
                    }
                    return std::string("since start");
                };
                std::string sinceLab = makeSinceLabel();
                TGraph g1; g1.SetTitle(Form("Timestamp (%s);Time [min];Trigger Number", sinceLab.c_str())); g1.SetMarkerStyle(20); g1.SetMarkerSize(0.6);
                TGraph g2; g2.SetTitle(Form("External timestamp (%s);Time [min];Trigger Number", sinceLab.c_str())); g2.SetMarkerStyle(20); g2.SetMarkerSize(0.6);
                Long64_t t0=0, e0=0; bool t0s=false, e0s=false; double xmin=0,xmax=0; double exmin=0, exmax=0;
                Long64_t last_ts = -1, last_ext = -1;
                for (Long64_t i=0;i<n;++i){
                    info->GetEntry(i);
                    // baseline = first non-zero value
                    if (!t0s && ts>0) { t0=ts; t0s=true; }
                    if (!e0s && info->GetBranch("ext_timestamp") && ext>0) { e0=ext; e0s=true; }
                    // compute minutes from baseline, guard negatives
                    double tmin = 0.0; if (t0s) { long double dt = (long double)ts - (long double)t0; if (dt<0) dt = 0; tmin = (double)(dt*TICK/60.0L); }
                    if(i==0){xmin=xmax=tmin;} else { if(tmin<xmin)xmin=tmin; if(tmin>xmax)xmax=tmin; }
                    double y = hasTrig ? (double)trig : (double)i; g1.SetPoint(g1.GetN(), tmin, y);
                    if (info->GetBranch("ext_timestamp") && e0s) {
                        long double de = (long double)ext - (long double)e0; if (de<0) de = 0; double emin = (double)(de*EXT_TICK/60.0L);
                        if(i==0){exmin=exmax=emin;} else { if(emin<exmin)exmin=emin; if(emin>exmax)exmax=emin; }
                        g2.SetPoint(g2.GetN(), emin, y);
                    }
                    last_ts = ts; last_ext = ext;
                }
                { TCanvas c("c_t1","time vs trigger",900,600); c.cd(); g1.Draw("AP"); c.Print(pdfPath.c_str()); }
                if (info->GetBranch("ext_timestamp") && g2.GetN()>0) { TCanvas c("c_t2","ext time vs trigger",900,600); c.cd(); g2.Draw("AP"); c.Print(pdfPath.c_str()); }
                std::cout << "Internal time span (min): [" << xmin << ", " << xmax << "]" << std::endl;
                if (info->GetBranch("ext_timestamp")) std::cout << "External time span (min): [" << exmin << ", " << exmax << "]" << std::endl;
                // Correlation ext vs int
                if (info->GetBranch("ext_timestamp")) {
                    TGraph g; g.SetTitle("External vs Internal time;Internal [min];External [min]"); g.SetMarkerStyle(20); g.SetMarkerSize(0.6);
                    Long64_t n2 = info->GetEntries(); Long64_t t0_=0,e0_=0; bool t0s_=false,e0s_=false; 
                    for (Long64_t i=0;i<n2;++i){ 
                        info->GetEntry(i);
                        if(!t0s_ && ts>0){t0_=ts;t0s_=true;} 
                        if(!e0s_ && ext>0){e0_=ext;e0s_=true;} 
                        double xi=(t0s_? ((ts>=t0_)? (double)((ts-t0_)*TICK/60.0) : 0.0) : 0.0);
                        double yi=(e0s_? ((ext>=e0_)? (double)((ext-e0_)*EXT_TICK/60.0) : 0.0) : 0.0);
                        g.SetPoint(g.GetN(), xi, yi);
                    }
                    if (g.GetN()>0) { TCanvas c("c_corr","ext vs int",800,600); c.cd(); g.Draw("AP"); c.Print(pdfPath.c_str()); }
                }
            }
            fr->Close();
        }
    }

    // Close PDF
    TCanvas cend; cend.Print((pdfPath+")").c_str());
    fin->Close();
    std::cout << "Wrote report to " << pdfPath << std::endl;
    return 0;
}
