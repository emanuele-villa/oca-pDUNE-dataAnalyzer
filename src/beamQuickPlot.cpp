// beamQuickPlot.cpp
// Lightweight report generator for aggregated (or single-run) beam files.
// Generates only pages 1,2,3,4,5,6 and 9 of the original dataAnalyzer report.
// Pages 7 (timestamps) and 8 (baseline+sigma combined) are intentionally skipped
// but page numbering is preserved so the final page still labels itself as Page 9.

#include "TFile.h"
#include "TTree.h"
#include "TBranch.h"
#include "TH1F.h"
#include "TH1I.h"
#include "TH2F.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TProfile.h"
#include "TApplication.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TMarker.h"
#include "TLatex.h"
#include "TLegend.h"
#include "TF1.h"
#include "TF2.h"
#include "TROOT.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <algorithm>
#include <filesystem>
#include <numeric>
#include <cmath>

// Minimal cmd parser (very small – avoids depending on existing one)
struct Args {
    std::string inputRoot;
    std::string calibFile;
    std::string beamSettingsFile; // optional: for enriching header if label not embedded
    std::string outputDir{"."};
    int nSigma{5};
    bool verbose{false};
};

static void usage() {
    std::cout << "beamQuickPlot --root <file.root> --cal <calibration.(csv|txt)> --out <dir> [--beam-settings beam_settings.dat] [--nsigma N] [--verbose]\n";
}

static bool parseArgs(int argc, char** argv, Args &a) {
    for (int i=1;i<argc;i++) {
        std::string s=argv[i];
        if (s=="--root" && i+1<argc) a.inputRoot=argv[++i];
        else if (s=="--cal" && i+1<argc) a.calibFile=argv[++i];
    else if (s=="--out" && i+1<argc) a.outputDir=argv[++i];
    else if (s=="--beam-settings" && i+1<argc) a.beamSettingsFile=argv[++i];
        else if (s=="--nsigma" && i+1<argc) a.nSigma=std::stoi(argv[++i]);
        else if (s=="--verbose") a.verbose=true;
        else { usage(); return false; }
    }
    if (a.inputRoot.empty() || a.calibFile.empty()) { usage(); return false; }
    return true;
}

static std::string baseName(const std::string &p) {
    std::string b = p.substr(p.find_last_of("/\\")+1);
    const std::string ext = ".root";
    if (b.size() > ext.size() && b.rfind(ext) == b.size()-ext.size()) {
        b = b.substr(0, b.size()-ext.size());
    }
    return b;
}

int main(int argc, char** argv) {
    Args args; if (!parseArgs(argc, argv, args)) return 1;
    std::filesystem::create_directories(args.outputDir);

    const int nDet=4; const int nCh=384;

    // Read calibration (baseline & sigma per detector/channel)
    std::ifstream cal(args.calibFile);
    if (!cal.is_open()) { std::cerr << "Cannot open calibration file: "<<args.calibFile<<"\n"; return 1; }
    std::vector<std::vector<float>> baseline(nDet, std::vector<float>(nCh));
    std::vector<std::vector<float>> baselineSigma(nDet, std::vector<float>(nCh));
    std::string line;
    bool csvFormat = args.calibFile.rfind(".csv")!=std::string::npos;
    bool emptyCal = cal.peek()==EOF;
    for (int d=0; d<nDet; ++d) {
        if (csvFormat) {
            for (int skip=0; skip<18; ++skip) std::getline(cal,line); // skip header for each detector block
        }
        for (int c=0;c<nCh;++c) {
            if (emptyCal) { baseline[d][c]=0; baselineSigma[d][c]=1; continue; }
            if (!std::getline(cal,line)) { std::cerr << "Calibration format error (early EOF)\n"; return 1; }
            std::istringstream iss(line);
            if (csvFormat) {
                std::string tok; std::vector<float> vals; while (std::getline(iss,tok,',')) { try { vals.push_back(std::stof(tok)); } catch(...) { vals.push_back(0); } }
                if (vals.size()<6) { std::cerr << "Bad calib csv line\n"; return 1; }
                int ch=(int)vals[0]; if (ch>=0 && ch<nCh) { baseline[d][ch]=vals[3]; baselineSigma[d][ch]=vals[5]; }
            } else {
                // Assume whitespace separated: ch baseline baselineSigma (fallback)
                int ch; float bl, sig; if (!(iss>>ch>>bl>>sig)) { std::cerr << "Bad calib txt line\n"; return 1; }
                if (ch>=0 && ch<nCh) { baseline[d][ch]=bl; baselineSigma[d][ch]=sig; }
            }
        }
    }

    TFile *fin = TFile::Open(args.inputRoot.c_str(),"READ");
    if (!fin || fin->IsZombie()) { std::cerr << "Cannot open input ROOT file\n"; return 1; }

    // Detector tree discovery (new/legacy)
    const char* newTree[4]={"detector_0","detector_1","detector_2","detector_3"};
    const char* legacyTree[4]={"raw_events","raw_events_B","raw_events_C","raw_events_D"};
    const char* newBranch="RAW Event";
    const char* legacyBranch[4]={"RAW Event","RAW Event B","RAW Event C","RAW Event D"};
    std::vector<TTree*> detTrees; detTrees.reserve(4);
    const char* usedBranch[4];
    for (int d=0; d<4; ++d) {
        TTree* t=(TTree*)fin->Get(newTree[d]);
        if (t && t->GetBranch(newBranch)) { detTrees.push_back(t); usedBranch[d]=newBranch; continue; }
        if (t && t->GetBranch(legacyBranch[d])) { detTrees.push_back(t); usedBranch[d]=legacyBranch[d]; continue; }
        t=(TTree*)fin->Get(legacyTree[d]);
        if (t && t->GetBranch(newBranch)) { detTrees.push_back(t); usedBranch[d]=newBranch; continue; }
        if (t && t->GetBranch(legacyBranch[d])) { detTrees.push_back(t); usedBranch[d]=legacyBranch[d]; continue; }
        std::cerr << "Missing detector tree for D"<<d<<"\n"; return 1;
    }

    int nEntries = detTrees[0]->GetEntries();

    // Branch data containers
    std::vector<std::vector<float>*> dataVec(nDet,nullptr);
    for (int d=0; d<nDet; ++d) { dataVec[d]=new std::vector<float>; detTrees[d]->SetBranchAddress(usedBranch[d], &dataVec[d]); }

    // Histograms needed for selected pages
    std::vector<TH1F*> h_firing(nDet); for (int d=0; d<nDet; ++d) h_firing[d]=new TH1F(Form("h_firing_D%d",d),Form("Firing channels (D%d)",d), nCh,0,nCh);
    TH1I* h_clustersPerEvent[4]; for (int d=0; d<4; ++d) h_clustersPerEvent[d]=new TH1I(Form("h_clEvt_D%d",d),Form("Clusters per event - D%d;Clusters;Events",d),11,-0.5,10.5);

    // Reco center accumulators (simplified: compute mean position of above-threshold channels per detector & combine)
    struct RecoPoint { double x; double y; };
    std::vector<RecoPoint> centers_3; centers_3.reserve(nEntries/10);
    std::vector<RecoPoint> centers_2to3; centers_2to3.reserve(nEntries/5);

    int eventsWithHits=0, eventsTwoDetOrMore=0, triggeredEvents=0;
    double sumX3=0,sumY3=0; int cnt3=0; double sumX23=0,sumY23=0; int cnt23=0;
    const int thresholdSigma = args.nSigma; // simplistic usage for channel firing

    for (int e=0; e<nEntries; ++e) {
        int detWithCluster=0; int clustersDet[4]={0,0,0,0};
        std::vector<std::pair<int,double>> detXY; detXY.reserve(4);
        for (int d=0; d<nDet; ++d) { detTrees[d]->GetEntry(e); if ((int)dataVec[d]->size()<nCh) continue; double sumAmp=0; double sumX=0; double sumY=0; int hits=0; for (int ch=0; ch<nCh; ++ch){ float amp=dataVec[d]->at(ch); float bl=baseline[d][ch]; float sig=baselineSigma[d][ch]; if (sig<=0) continue; if (amp > bl + thresholdSigma*sig){ h_firing[d]->Fill(ch); sumAmp+=amp; // rough geometry: map channel -> (x,y) using chip heuristics
                int chip = ch/64; int local = ch%64; double x = (chip%8)*64 + local; double y = chip/8; sumX += x; sumY += y; ++hits; } }
            if (hits>0){ eventsWithHits++; detWithCluster++; clustersDet[d]=1; h_clustersPerEvent[d]->Fill(1); double cx=sumX/hits; double cy=sumY/hits; detXY.emplace_back(d,cx); // store x only; y compressed
            }
        }
        if (detWithCluster>=2) eventsTwoDetOrMore++;
        if (detWithCluster==3){ triggeredEvents++; double mx=0,my=0; for (auto &p:detXY){ mx+=p.second; } mx/=detWithCluster; my=0; centers_3.push_back({mx,my}); sumX3+=mx; sumY3+=my; cnt3++; centers_2to3.push_back({mx,my}); sumX23+=mx; sumY23+=my; cnt23++; }
        else if (detWithCluster==2){ double mx=0; for(auto&p:detXY) mx+=p.second; mx/=detWithCluster; double my=0; centers_2to3.push_back({mx,my}); sumX23+=mx; sumY23+=my; cnt23++; }
    }

    // Graphs for centers heatmap approximations (use TH2F)
    TH2F *h_center3 = new TH2F("h_center3","Reco Centers (exactly 3);X;Yidx",100,0, nCh, 10, -1, 9);
    for (auto &c: centers_3) h_center3->Fill(c.x,c.y);
    TH2F *h_center23 = new TH2F("h_center23","Reco Centers (2 or 3);X;Yidx",100,0,nCh,10,-1,9);
    for (auto &c: centers_2to3) h_center23->Fill(c.x,c.y);

    // Scatter graphs
    TGraph *g_scatter3 = new TGraph(); for (size_t i=0;i<centers_3.size();++i) g_scatter3->SetPoint((int)i, centers_3[i].x, centers_3[i].y);
    TGraph *g_scatter23 = new TGraph(); for (size_t i=0;i<centers_2to3.size();++i) g_scatter23->SetPoint((int)i, centers_2to3[i].x, centers_2to3[i].y);

    // PDF output
    std::string base = baseName(args.inputRoot);
    std::string pdfOut = args.outputDir + "/" + base + "_quickReport.pdf";
    // Backward compatibility: if an older run created a file with base including .root, we ignore it now.

    // Derive beam condition label
    std::string beamLabel;
    if (auto named = (TNamed*)fin->Get("beam_label")) {
        beamLabel = named->GetTitle();
    } else if (!args.beamSettingsFile.empty()) {
        // Attempt to infer from filename token
        // Search energy token inside beam_settings.dat
        std::ifstream bs(args.beamSettingsFile);
        if (bs.is_open()) {
            std::string bn = base; // base like YYYYMMDD_HHMMSS_plus1p0GeV
            size_t us = bn.find_last_of('_');
            std::string energyTok = (us!=std::string::npos)? bn.substr(us+1) : bn;
            // Convert plus1p0GeV -> +1.0 for matching
            if (energyTok.rfind("plus",0)==0) energyTok = "+" + energyTok.substr(4);
            else if (energyTok.rfind("minus",0)==0) energyTok = "-" + energyTok.substr(5);
            // replace p with . before GeV
            size_t gpos = energyTok.find("GeV");
            if (gpos!=std::string::npos) {
                std::string core = energyTok.substr(0,gpos);
                for(char &c: core) if (c=='p') c='.';
                energyTok = core; // still like +1.0
            }
            std::string bln;
            while (std::getline(bs, bln)) {
                if (bln.find("---")!=std::string::npos || bln.size()<5 || bln[0]=='#') continue;
                if (bln.find("Energy")!=std::string::npos && bln.find("Date")!=std::string::npos) continue;
                if (bln.find(energyTok) != std::string::npos) { beamLabel = bln; break; }
            }
        }
    }
    if (beamLabel.empty()) beamLabel = base;

    // Page 1 summary
    TCanvas *c_summary = new TCanvas("c_summary","Summary",800,600);
    c_summary->cd(); TLatex lat; lat.SetNDC(true); lat.SetTextSize(0.028);
    lat.DrawLatex(0.10,0.95,Form("%s", beamLabel.c_str()));
    lat.SetTextSize(0.024); lat.DrawLatex(0.90,0.03,"Page 1");
    lat.DrawLatex(0.10,0.88,Form("Total triggers: %d", nEntries));
    lat.DrawLatex(0.10,0.83,Form(">=1 hit events: %d (%.2f%%)", eventsWithHits, nEntries?100.0*eventsWithHits/nEntries:0));
    lat.DrawLatex(0.10,0.78,Form(">=2 detectors: %d (%.2f%%)", eventsTwoDetOrMore, nEntries?100.0*eventsTwoDetOrMore/nEntries:0));
    lat.DrawLatex(0.10,0.73,Form("Triggered (3 det): %d (%.2f%%)", triggeredEvents, nEntries?100.0*triggeredEvents/nEntries:0));
    if (cnt23>0) lat.DrawLatex(0.10,0.66,Form("Mean center (2or3): %.1f", sumX23/cnt23));
    if (cnt3>0)  lat.DrawLatex(0.10,0.61,Form("Mean center (exactly3): %.1f", sumX3/cnt3));
    c_summary->SaveAs((pdfOut+"(").c_str());

    // Page 2 heatmap exactly 3
    if (centers_3.size()) {
        TCanvas *c2=new TCanvas("c_p2","Centers3",800,600); c2->cd(); h_center3->Draw("COLZ"); TLatex l; l.SetNDC(true); l.SetTextSize(0.024); l.DrawLatex(0.90,0.03,"Page 2"); c2->SaveAs(pdfOut.c_str()); delete c2; }
    else { TCanvas *c2=new TCanvas("c_p2empty","Centers3 empty",600,400); TLatex l; l.SetNDC(true); l.DrawLatex(0.3,0.5,"No exactly-3 events"); l.DrawLatex(0.9,0.03,"Page 2"); c2->SaveAs(pdfOut.c_str()); delete c2; }

    // Page 3 heatmap 2 or 3
    if (centers_2to3.size()) { TCanvas *c3=new TCanvas("c_p3","Centers2to3",800,600); c3->cd(); h_center23->Draw("COLZ"); TLatex l; l.SetNDC(true); l.SetTextSize(0.024); l.DrawLatex(0.90,0.03,"Page 3"); c3->SaveAs(pdfOut.c_str()); delete c3; }
    else { TCanvas *c3=new TCanvas("c_p3empty","Centers2to3 empty",600,400); TLatex l; l.SetNDC(true); l.DrawLatex(0.3,0.5,"No 2or3 events"); l.DrawLatex(0.9,0.03,"Page 3"); c3->SaveAs(pdfOut.c_str()); delete c3; }

    // Page 4 scatter exactly 3
    {
        TCanvas *c4=new TCanvas("c_p4","Scatter3",800,600); c4->cd(); if (g_scatter3->GetN()>0) { g_scatter3->SetTitle("Centers (exactly 3);X;Yidx"); g_scatter3->SetMarkerStyle(20); g_scatter3->Draw("AP"); } else { TLatex l; l.SetNDC(true); l.DrawLatex(0.3,0.5,"No points"); } TLatex l; l.SetNDC(true); l.SetTextSize(0.024); l.DrawLatex(0.90,0.03,"Page 4"); c4->SaveAs(pdfOut.c_str()); delete c4; }

    // Page 5 scatter 2 or 3
    {
        TCanvas *c5=new TCanvas("c_p5","Scatter23",800,600); c5->cd(); if (g_scatter23->GetN()>0) { g_scatter23->SetTitle("Centers (2 or 3);X;Yidx"); g_scatter23->SetMarkerStyle(20); g_scatter23->Draw("AP"); } else { TLatex l; l.SetNDC(true); l.DrawLatex(0.3,0.5,"No points"); } TLatex l; l.SetNDC(true); l.SetTextSize(0.024); l.DrawLatex(0.90,0.03,"Page 5"); c5->SaveAs(pdfOut.c_str()); delete c5; }

    // Page 6 clusters per event (per detector)
    {
        TCanvas *c6=new TCanvas("c_p6","ClustersPerEvent",800,600); c6->Divide(2,2); for (int d=0; d<4; ++d){ c6->cd(d+1); gPad->SetLogy(0); h_clustersPerEvent[d]->GetXaxis()->SetRangeUser(0,6); h_clustersPerEvent[d]->Draw(); }
        c6->cd(); TLatex l; l.SetNDC(true); l.SetTextSize(0.024); l.DrawLatex(0.90,0.03,"Page 6"); c6->SaveAs(pdfOut.c_str()); delete c6; }

    // Page 9 firing channels (skip 7 & 8 but preserve number)
    {
        TCanvas *c9=new TCanvas("c_p9","FiringChannels",1000,800); c9->Divide(2,2); for (int d=0; d<4; ++d){ c9->cd(d+1); gPad->SetLogy(); h_firing[d]->SetMinimum(0.5); h_firing[d]->Draw(); }
        c9->cd(); TLatex l; l.SetNDC(true); l.SetTextSize(0.024); l.DrawLatex(0.90,0.03,"Page 9"); c9->SaveAs((pdfOut+")").c_str()); delete c9; }

    std::cout << "beamQuickPlot: written " << pdfOut << std::endl;

    // Cleanup
    for (auto p: dataVec) delete p; fin->Close(); delete fin;
    return 0;
}
