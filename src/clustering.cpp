// clustering.cpp
// Standalone clustering app: reads converted ROOT, computes simple clusters, writes *_clusters.root
// No canvases, no interactive ROOT

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TBranch.h"
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <filesystem>

#include "ocaEvent.h"

struct EventClusters {
    // Per-detector clusters as vector of [start,end] channel ranges (inclusive)
    std::vector<int> d0_starts, d0_ends;
    std::vector<int> d1_starts, d1_ends;
    std::vector<int> d2_starts, d2_ends;
    std::vector<int> d3_starts, d3_ends;
};

static std::string inferOutputPath(const std::string &input) {
    std::filesystem::path p(input);
    std::string stem = p.stem().string();
    // If input ends with _converted.root, replace with _clusters.root, else append
    if (stem.size() > 10 && stem.rfind("_converted", std::string::npos) == stem.size() - 10) {
        stem = stem.substr(0, stem.size() - 10);
    }
    std::filesystem::path out = p.parent_path() / (stem + "_clusters.root");
    return out.string();
}

int main(int argc, char** argv) {
    // Args: -r inputConvertedRoot [-o outputClustersRoot] [-s nSigma] [-c calibFile]
    std::string inputFile;
    std::string outputFile;
    int nSigma = 5; // consistent with scripts default
    std::string calibPath; // optional (not required for this lightweight clustering)
    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        if ((a == "-r" || a == "--root-file") && i+1<argc) { inputFile = argv[++i]; }
        else if ((a == "-o" || a == "--output") && i+1<argc) { outputFile = argv[++i]; }
        else if ((a == "-s" || a == "--n-sigma") && i+1<argc) { nSigma = std::max(1, atoi(argv[++i])); }
        else if ((a == "-c" || a == "--cal-file") && i+1<argc) { calibPath = argv[++i]; }
        else if (a == "-h" || a == "--help") {
            std::cout << "Usage: clustering -r input_converted.root [-o output_clusters.root] [-s nSigma]" << std::endl;
            return 0;
        }
    }
    if (inputFile.empty()) { std::cerr << "Error: missing -r input ROOT file" << std::endl; return 1; }
    if (outputFile.empty()) outputFile = inferOutputPath(inputFile);

    TFile* fin = TFile::Open(inputFile.c_str(), "READ");
    if (!fin || !fin->IsOpen()) { std::cerr << "Failed to open input file: " << inputFile << std::endl; return 1; }

    // Prefer new naming: detector_0..3 with branch name "RAW Event";
    // Fallback legacy: raw_events, raw_events_B, raw_events_C, raw_events_D with branches "RAW Event", "RAW Event B", ...
    const char* newTreeNames[4]   = {"detector_0","detector_1","detector_2","detector_3"};
    const char* legacyTreeNames[4]= {"raw_events","raw_events_B","raw_events_C","raw_events_D"};
    const char* newBranchName = "RAW Event";
    const char* legacyBranchNames[4] = {"RAW Event","RAW Event B","RAW Event C","RAW Event D"};
    TTree* t[4] = {nullptr,nullptr,nullptr,nullptr};
    const char* useBranch[4] = {nullptr,nullptr,nullptr,nullptr};
    const char* useTree[4] = {nullptr,nullptr,nullptr,nullptr};
    // For each detector, try combinations in priority order and pick the first that works
    std::vector<unsigned int>* buf[4] = {nullptr,nullptr,nullptr,nullptr};
    auto tryBind = [&](int d)->bool{
        // 1) New tree + new branch
        TTree* tt = (TTree*)fin->Get(newTreeNames[d]);
        if (tt && tt->GetBranch(newBranchName)) {
            t[d] = tt; useTree[d] = newTreeNames[d]; useBranch[d] = newBranchName;
            buf[d] = new std::vector<unsigned int>();
            return (t[d]->SetBranchAddress(useBranch[d], &buf[d]) >= 0);
        }
        // 2) New tree + legacy branch (hybrid files)
        const char* legB = legacyBranchNames[d];
        if (tt && tt->GetBranch(legB)) {
            t[d] = tt; useTree[d] = newTreeNames[d]; useBranch[d] = legB;
            buf[d] = new std::vector<unsigned int>();
            return (t[d]->SetBranchAddress(useBranch[d], &buf[d]) >= 0);
        }
        // 3) Legacy tree + new branch (unlikely but cheap to try)
        tt = (TTree*)fin->Get(legacyTreeNames[d]);
        if (tt && tt->GetBranch(newBranchName)) {
            t[d] = tt; useTree[d] = legacyTreeNames[d]; useBranch[d] = newBranchName;
            buf[d] = new std::vector<unsigned int>();
            return (t[d]->SetBranchAddress(useBranch[d], &buf[d]) >= 0);
        }
        // 4) Legacy tree + legacy branch
        if (tt && tt->GetBranch(legacyBranchNames[d])) {
            t[d] = tt; useTree[d] = legacyTreeNames[d]; useBranch[d] = legacyBranchNames[d];
            buf[d] = new std::vector<unsigned int>();
            return (t[d]->SetBranchAddress(useBranch[d], &buf[d]) >= 0);
        }
        return false;
    };
    for (int d=0; d<4; ++d) {
        if (!tryBind(d)) {
            std::cerr << "Could not bind detector " << d << ": tried new/legacy trees and branches without success." << std::endl;
            return 2;
        }
    }

    // Optional: read event_info for length matching
    TTree* info = (TTree*)fin->Get("event_info");
    Long64_t nEntries = t[0]->GetEntries();
    for (int d=1; d<4; ++d) nEntries = std::min(nEntries, t[d]->GetEntries());
    if (info) nEntries = std::min<Long64_t>(nEntries, info->GetEntries());

    // Output
    TFile* fout = TFile::Open(outputFile.c_str(), "RECREATE");
    if (!fout || !fout->IsOpen()) { std::cerr << "Failed to create output file: " << outputFile << std::endl; return 3; }
    TTree* clustersT = new TTree("clusters", "Clusters per event (simple contiguous-hit grouping)");
    EventClusters ev;
    clustersT->Branch("d0_starts", &ev.d0_starts);
    clustersT->Branch("d0_ends",   &ev.d0_ends);
    clustersT->Branch("d1_starts", &ev.d1_starts);
    clustersT->Branch("d1_ends",   &ev.d1_ends);
    clustersT->Branch("d2_starts", &ev.d2_starts);
    clustersT->Branch("d2_ends",   &ev.d2_ends);
    clustersT->Branch("d3_starts", &ev.d3_starts);
    clustersT->Branch("d3_ends",   &ev.d3_ends);

    // Basic per-detector histos
    TH1F* h_hitsPerEvent = new TH1F("h_hitsPerEvent", "Hits per event;Hits;Events", 51, -0.5, 50.5);
    TH1F* h_clustersPerEvent[4];
    for (int d=0; d<4; ++d) h_clustersPerEvent[d] = new TH1F(Form("h_clustersPerEvent_D%d", d), Form("Clusters/event D%d;Clusters;Events", d), 11, -0.5, 10.5);

    // Light-weight per-channel occupancy
    TH1F* h_firingChannels[4];
    for (int d=0; d<4; ++d) h_firingChannels[d] = new TH1F(Form("h_firingChannels_D%d", d), Form("Firing channels D%d;Channel;Counts", d), 384, 0, 384);

    // Event loop: compute per-detector hits above threshold and group contiguous channels
    for (Long64_t i=0; i<nEntries; ++i) {
        for (int d=0; d<4; ++d) t[d]->GetEntry(i);
        // For each detector, estimate baseline from median and sigma from MAD; then mark channels with (val - baseline) > nSigma*sigma
        auto findClusters = [&](const std::vector<unsigned int>& wave) {
            const int N = (int)wave.size();
            std::vector<int> hits; hits.reserve(64);
            if (N <= 0) return std::vector<std::pair<int,int>>{};
            // Baseline estimate: median of values
            std::vector<unsigned int> tmp = wave; std::nth_element(tmp.begin(), tmp.begin()+tmp.size()/2, tmp.end());
            double med = tmp[tmp.size()/2];
            // Robust sigma: MAD * 1.4826
            std::vector<double> absdev; absdev.reserve(N);
            for (int k=0;k<N;++k) absdev.push_back(fabs((double)wave[k] - med));
            std::nth_element(absdev.begin(), absdev.begin()+absdev.size()/2, absdev.end());
            double mad = absdev[absdev.size()/2];
            double sigma = (mad > 0 ? 1.4826*mad : 1.0);
            double thr = med + nSigma * sigma;
            for (int ch=0; ch<N; ++ch) {
                if ((double)wave[ch] > thr) hits.push_back(ch);
            }
            if (hits.empty()) return std::vector<std::pair<int,int>>{};
            std::sort(hits.begin(), hits.end()); hits.erase(std::unique(hits.begin(), hits.end()), hits.end());
            // Group contiguous
            std::vector<std::pair<int,int>> clusters; clusters.reserve(8);
            int s = hits.front(), e = hits.front();
            for (size_t j=1; j<hits.size(); ++j) {
                if (hits[j] == e+1) { e = hits[j]; }
                else { clusters.emplace_back(s,e); s = e = hits[j]; }
            }
            clusters.emplace_back(s,e);
            return clusters;
        };

        ev.d0_starts.clear(); ev.d0_ends.clear();
        ev.d1_starts.clear(); ev.d1_ends.clear();
        ev.d2_starts.clear(); ev.d2_ends.clear();
        ev.d3_starts.clear(); ev.d3_ends.clear();
        int hitsSum = 0;
        for (int d=0; d<4; ++d) {
            const auto& wave = *buf[d];
            auto clusters = findClusters(wave);
            for (auto &c : clusters) {
                if (d==0) { ev.d0_starts.push_back(c.first); ev.d0_ends.push_back(c.second); }
                else if (d==1) { ev.d1_starts.push_back(c.first); ev.d1_ends.push_back(c.second); }
                else if (d==2) { ev.d2_starts.push_back(c.first); ev.d2_ends.push_back(c.second); }
                else { ev.d3_starts.push_back(c.first); ev.d3_ends.push_back(c.second); }
            }
            // Histos
            for (auto &c : clusters) {
                for (int ch=c.first; ch<=c.second; ++ch) { h_firingChannels[d]->Fill(ch); hitsSum++; }
            }
            h_clustersPerEvent[d]->Fill((int)clusters.size());
        }
        h_hitsPerEvent->Fill(hitsSum);
        clustersT->Fill();
    }

    fout->cd();
    clustersT->Write();
    h_hitsPerEvent->Write();
    for (int d=0; d<4; ++d) { h_clustersPerEvent[d]->Write(); h_firingChannels[d]->Write(); }
    fout->Close();
    fin->Close();
    std::cout << "Wrote clusters to " << outputFile << std::endl;
    return 0;
}
