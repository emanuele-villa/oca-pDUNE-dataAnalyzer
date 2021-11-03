#include <iostream>
#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include "TH1.h"
#include "TGraph.h"
#include "anyoption.h"
#include <ctime>
#include <tuple>

#include "PAPERO.h"

template <typename T>
void print(std::vector<T> const &v)
{
    for (auto i : v)
    {
        std::cout << std::hex << i << ' ' << std::endl;
    }
    std::cout << '\n';
}

template <typename T>
std::vector<T> reorder(std::vector<T> const &v)
{
    std::vector<T> reordered_vec(v.size());
    int j = 0;
    constexpr int order[] = {1, 0, 3, 2, 5, 4, 7, 6, 9, 8};
    for (int ch = 0; ch < 128; ch++)
    {
        for (int adc : order)
        {
            reordered_vec.at(adc * 128 + ch) = v.at(j);
            j++;
        }
    }
    return reordered_vec;
}

template <typename T>
std::vector<T> reorder_DAMPE(std::vector<T> const &v)
{
    std::vector<T> reordered_vec(v.size());
    int j = 0;
    constexpr int order[] = {1, 0};
    for (int ch = 0; ch < 192; ch++)
    {
        for (int adc : order)
        {
            reordered_vec.at(adc * 192 + ch) = v.at(j);
            j++;
        }
    }
    return reordered_vec;
}

AnyOption *opt; //Handle the option input

int main(int argc, char *argv[])
{
    opt = new AnyOption();
    opt->addUsage("Usage: ./PAPERO_compress [options] raw_data_file output_rootfile");
    opt->addUsage("");
    opt->addUsage("Options: ");
    opt->addUsage("  -h, --help       ................................. Print this help ");
    opt->addUsage("  -v               ................................. Verbose ");
    opt->addUsage("  --boards         ................................. Number of DE10Nano boards connected ");
    opt->setOption("boards");

    opt->setFlag("help", 'h');
    opt->setFlag("v");

    opt->processFile("./options.txt");
    opt->processCommandArgs(argc, argv);

    TFile *foutput;

    if (!opt->hasOptions())
    { /* print usage if no options */
        opt->printUsage();
        delete opt;
        return 2;
    }

    bool verbose = false;


    if (opt->getValue("v"))
    {
        verbose = true;
    }

    //Open binary data file
    std::fstream file(opt->getArgv(0), std::ios::in | std::ios::out | std::ios::binary);
    if (file.fail())
    {
        std::cout << "ERROR: can't open input file" << std::endl; // file could not be opened
        return 2;
    }

    std::cout << " " << std::endl;
    std::cout << "Processing file " << opt->getArgv(0) << std::endl;

    //Create output ROOT file
    TString output_filename = opt->getArgv(1);
    foutput = new TFile(output_filename.Data(), "RECREATE", "PAPERO data");
    foutput->cd();

    //Initialize TTree(s)
    std::vector<unsigned int> raw_event_buffer;

    TTree *raw_events = new TTree("raw_events", "raw_events");
    std::vector<unsigned int> raw_event;
    raw_events->Branch("RAW Event", &raw_event);
    raw_events->SetAutoSave(0);
    TTree *raw_events_B = new TTree("raw_events_B", "raw_events_B");
    std::vector<unsigned int> raw_event_B;
    raw_events_B->Branch("RAW Event B", &raw_event_B);
    raw_events_B->SetAutoSave(0);

    TTree *raw_events_C = new TTree("raw_events_C", "raw_events_C");
    std::vector<unsigned int> raw_event_C;
    raw_events_C->Branch("RAW Event C", &raw_event_C);
    raw_events_C->SetAutoSave(0);
    TTree *raw_events_D = new TTree("raw_events_D", "raw_events_D");
    std::vector<unsigned int> raw_event_D;
    raw_events_D->Branch("RAW Event D", &raw_event_D);
    raw_events_D->SetAutoSave(0);

    TTree *raw_events_E = new TTree("raw_events_E", "raw_events_E");
    std::vector<unsigned int> raw_event_E;
    raw_events_E->Branch("RAW Event E", &raw_event_E);
    raw_events_E->SetAutoSave(0);
    TTree *raw_events_F = new TTree("raw_events_F", "raw_events_F");
    std::vector<unsigned int> raw_event_F;
    raw_events_F->Branch("RAW Event F", &raw_event_F);
    raw_events_F->SetAutoSave(0);

    TTree *raw_events_G = new TTree("raw_events_G", "raw_events_G");
    std::vector<unsigned int> raw_event_G;
    raw_events_G->Branch("RAW Event G", &raw_event_G);
    raw_events_G->SetAutoSave(0);
    TTree *raw_events_H = new TTree("raw_events_H", "raw_events_H");
    std::vector<unsigned int> raw_event_H;
    raw_events_H->Branch("RAW Event H", &raw_event_H);
    raw_events_H->SetAutoSave(0);

    TTree *raw_events_I = new TTree("raw_events_I", "raw_events_I");
    std::vector<unsigned int> raw_event_I;
    raw_events_I->Branch("RAW Event I", &raw_event_I);
    raw_events_I->SetAutoSave(0);
    TTree *raw_events_J = new TTree("raw_events_J", "raw_events_J");
    std::vector<unsigned int> raw_event_J;
    raw_events_J->Branch("RAW Event J", &raw_event_J);
    raw_events_J->SetAutoSave(0);

    TTree *raw_events_K = new TTree("raw_events_K", "raw_events_K");
    std::vector<unsigned int> raw_event_K;
    raw_events_K->Branch("RAW Event K", &raw_event_K);
    raw_events_K->SetAutoSave(0);
    TTree *raw_events_L = new TTree("raw_events_L", "raw_events_L");
    std::vector<unsigned int> raw_event_L;
    raw_events_L->Branch("RAW Event L", &raw_event_L);
    raw_events_L->SetAutoSave(0);

    TTree *raw_events_M = new TTree("raw_events_M", "raw_events_M");
    std::vector<unsigned int> raw_event_M;
    raw_events_M->Branch("RAW Event M", &raw_event_M);
    raw_events_M->SetAutoSave(0);
    TTree *raw_events_N = new TTree("raw_events_N", "raw_events_N");
    std::vector<unsigned int> raw_event_N;
    raw_events_N->Branch("RAW Event N", &raw_event_N);
    raw_events_N->SetAutoSave(0);

    //Find if there is an offset before first event
    unsigned int offset = 0;
    offset = seek_run_header(file, offset, verbose);
    int padding_offset = 0;

    //Read raw events and write to TTree
    bool is_good = false;
    int evtnum = 0;
    int boards = 0;
    unsigned long fw_version = 0;
    int board_id = -1;
    int trigger_id = -1;
    int evt_size = 0;
    unsigned short timestamp = 0;
    int boards_read = 0;
    float mean_rate = 0;
    std::tuple<bool, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, int> evt_retValues;


    if (!opt->getValue("boards"))
    {
        std::cout << "ERROR: you need to provide the number of boards connected" << std::endl;
        return 2;
    }
    else
    {
        boards = atoi(opt->getValue("boards"));
    }

    while (!file.eof())
    {
        evt_retValues = read_de10_header(file, offset, verbose);
        is_good = std::get<0>(evt_retValues);

        if (is_good)
        {
            boards_read++;
            evt_size = std::get<1>(evt_retValues);
            fw_version = std::get<2>(evt_retValues);
            //evtnum = std::get<3>(evt_retValues);
            board_id = std::get<4>(evt_retValues);
            timestamp = std::get<5>(evt_retValues);
            trigger_id = std::get<6>(evt_retValues);
            offset = std::get<7>(evt_retValues);

            std::cout << "\r\tReading event " << evtnum << std::flush;


            if (verbose)
            {
                std::cout << "\tBoard ID " << board_id << std::endl;
                std::cout << "\tBoards read " << boards_read << " out of " << boards << std::endl;
                std::cout << "\tTrigger ID " << trigger_id << std::endl;
                std::cout << "\tFW version is: " << std::hex << fw_version << std::dec << std::endl;
                std::cout << "\tEvt lenght: " << evt_size << std::endl;
            }

            if (fw_version == 0xffffffff9fd68b40)
            {
                //std::cout << "\tLADDERONE!!!" << std::endl;
                padding_offset = 1024;
                board_id = board_id - 300;
                //std::cout << "\tFixed Board ID " << board_id << std::endl;
                raw_event_buffer = reorder_DAMPE(read_event(file, offset, evt_size, verbose));
            }
            else
            {
                padding_offset = 0;
                raw_event_buffer = reorder(read_event(file, offset, evt_size, verbose));
            }

            if (board_id == 0)
            {
                raw_event = std::vector<unsigned int>(raw_event_buffer.begin(), raw_event_buffer.begin() + raw_event_buffer.size() / 2);
                raw_event_B = std::vector<unsigned int>(raw_event_buffer.begin() + raw_event_buffer.size() / 2, raw_event_buffer.end());
                raw_events->Fill();
                raw_events_B->Fill();
            }
            else if (board_id == 1)
            {
                raw_event_C = std::vector<unsigned int>(raw_event_buffer.begin(), raw_event_buffer.begin() + raw_event_buffer.size() / 2);
                raw_event_D = std::vector<unsigned int>(raw_event_buffer.begin() + raw_event_buffer.size() / 2, raw_event_buffer.end());
                raw_events_C->Fill();
                raw_events_D->Fill();
            }
            else if (board_id == 2)
            {
                raw_event_E = std::vector<unsigned int>(raw_event_buffer.begin(), raw_event_buffer.begin() + raw_event_buffer.size() / 2);
                raw_events_E->Fill();
                raw_event_F = std::vector<unsigned int>(raw_event_buffer.begin() + raw_event_buffer.size() / 2, raw_event_buffer.end());
                raw_events_F->Fill();
            }
            else if (board_id == 3)
            {
                raw_event_G = std::vector<unsigned int>(raw_event_buffer.begin(), raw_event_buffer.begin() + raw_event_buffer.size() / 2);
                raw_event_H = std::vector<unsigned int>(raw_event_buffer.begin() + raw_event_buffer.size() / 2, raw_event_buffer.end());
                raw_events_G->Fill();
                raw_events_H->Fill();
            }
            else if (board_id == 4)
            {
                raw_event_I = std::vector<unsigned int>(raw_event_buffer.begin(), raw_event_buffer.begin() + raw_event_buffer.size() / 2);
                raw_event_J = std::vector<unsigned int>(raw_event_buffer.begin() + raw_event_buffer.size() / 2, raw_event_buffer.end());
                raw_events_I->Fill();
                raw_events_J->Fill();
            }
            else if (board_id == 5)
            {
                raw_event_K = std::vector<unsigned int>(raw_event_buffer.begin(), raw_event_buffer.begin() + raw_event_buffer.size() / 2);
                raw_event_L = std::vector<unsigned int>(raw_event_buffer.begin() + raw_event_buffer.size() / 2, raw_event_buffer.end());
                raw_events_K->Fill();
                raw_events_L->Fill();
            }
            else if (board_id == 6)
            {
                raw_event_M = std::vector<unsigned int>(raw_event_buffer.begin(), raw_event_buffer.begin() + raw_event_buffer.size() / 2);
                raw_event_N = std::vector<unsigned int>(raw_event_buffer.begin() + raw_event_buffer.size() / 2, raw_event_buffer.end());
                raw_events_M->Fill();
                raw_events_N->Fill();
            }

            if (boards_read == boards)
            {
                boards_read = 0;
                evtnum++;
                offset = (int)file.tellg() + padding_offset + 8;
            }
            else
            {
                offset = (int)file.tellg() + padding_offset + 4;
            }
        }
        else
        {
            break;
        }
    }

    std::cout << "\n\tClosing file after " << evtnum << " events" << std::endl;

    if (raw_events->GetEntries())
        raw_events->Write();

    if (raw_events_B->GetEntries())
        raw_events_B->Write();

    if (raw_events_C->GetEntries())
        raw_events_C->Write();

    if (raw_events_D->GetEntries())
        raw_events_D->Write();

    if (raw_events_E->GetEntries())
        raw_events_E->Write();

    if (raw_events_F->GetEntries())
        raw_events_F->Write();

    if (raw_events_G->GetEntries())
        raw_events_G->Write();

    if (raw_events_H->GetEntries())
        raw_events_H->Write();

    if (raw_events_I->GetEntries())
        raw_events_I->Write();

    if (raw_events_J->GetEntries())
        raw_events_J->Write();

    if (raw_events_K->GetEntries())
        raw_events_K->Write();

    if (raw_events_L->GetEntries())
        raw_events_L->Write();

    if (raw_events_M->GetEntries())
        raw_events_M->Write();

    if (raw_events_N->GetEntries())
        raw_events_N->Write();

    foutput->Close();
    file.close();
    return 0;
}
