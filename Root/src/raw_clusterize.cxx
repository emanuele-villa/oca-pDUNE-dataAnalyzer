#include "TChain.h"
#include "TFile.h"
#include "TF1.h"
#include "TH1.h"
#include "TH2.h"
#include "TGraph.h"
#include "TTree.h"
#include <iostream>
#include "environment.h"
#include <algorithm>

#include "anyoption.h"
#include "event.h"

#if OMP_ == 1
#include "omp.h"
#endif

AnyOption *opt; //Handle the option input

calib update_pedestals(TH1D **hADC, int NChannels, calib cal)
{
  calib new_calibration;

  std::vector<float> pedestals;
  float mean_pedestal = 0;
  float rms_pedestal = 0;
  std::vector<float> rsigma;
  float mean_rsigma = 0;
  float rms_rsigma = 0;
  std::vector<float> sigma;
  float mean_sigma = 0;
  float rms_sigma = 0;

  TF1 *fittedgaus;

  for (int ch = 0; ch < NChannels; ch++)
  {
    //Fitting histos with gaus to compute ped and raw_sigma
    if (hADC[ch]->GetEntries())
    {
      hADC[ch]->Fit("gaus", "QS");
      fittedgaus = (TF1 *)hADC[ch]->GetListOfFunctions()->FindObject("gaus");
      pedestals.push_back(fittedgaus->GetParameter(1));
      rsigma.push_back(fittedgaus->GetParameter(2));
    }
    else
    {
      pedestals.push_back(0);
      rsigma.push_back(0);
    }
  }

  new_calibration = (calib){.ped = pedestals, .rsig = rsigma, .sig = cal.sig, .status = cal.status};
  return new_calibration;
}

int main(int argc, char *argv[])
{
  gErrorIgnoreLevel = kWarning;
  bool symmetric = false;
  bool absolute = false;
  bool verb = false;
  bool invert = false;
  bool dynped = false;

  float highthreshold = 3.5;
  float lowthreshold = 1.0;
  int symmetricwidth = 0;
  int cntype = 0;
  int maxCN = 999;

  int NChannels = 384;
  int NVas = 6;
  int minStrip = 0;
  int maxStrip = 383;
  float sensor_pitch = 0.150;

  bool newDAQ = false;
  int side = 0;
  int board = 0;

  opt = new AnyOption();
  opt->addUsage("Usage: ./raw_clusterize [options] [arguments] rootfile1 rootfile2 ...");
  opt->addUsage("");
  opt->addUsage("Options: ");
  opt->addUsage("  -h, --help       ................................. Print this help ");
  opt->addUsage("  -v, --verbose    ................................. Verbose ");
  opt->addUsage("  --nevents        ................................. Number of events to process ");
  opt->addUsage("  --version        ................................. 1212 for 6VA or 1313 for 10VA miniTRB or 2020 for FOOT DAQ");
  opt->addUsage("  --output         ................................. Output ROOT file ");
  opt->addUsage("  --calibration    ................................. Calibration file ");
  opt->addUsage("  --dynped         ................................. Enable dynamic pedestals ");
  opt->addUsage("  --highthreshold  ................................. High threshold used in the clusterization ");
  opt->addUsage("  --lowthreshold   ................................. Low threshold used in the clusterization ");
  opt->addUsage("  -s, --symmetric  ................................. Use symmetric cluster instead of double threshold ");
  opt->addUsage("  --symmetricwidth ................................. Width of symmetric clusters ");
  opt->addUsage("  -a, --absolute   ................................. Use absolute ADC value instead of S/N for thresholds ");
  opt->addUsage("  --cn             ................................. CN algorithm selection (0,1,2) ");
  opt->addUsage("  --maxcn          ................................. Max CN for a good event");
  opt->addUsage("  --minstrip       ................................. Minimun strip number to analyze");
  opt->addUsage("  --maxstrip       ................................. Maximum strip number to analyze");
  opt->addUsage("  --board          ................................. ADC board to analyze (0, 1, 2)");
  opt->addUsage("  --side           ................................. Sensor side for new FOOT DAQ (0, 1)");
  opt->addUsage("  --invert         ................................. To search for negative signal peaks (prototype ADC board)");

  opt->setFlag("help", 'h');
  opt->setFlag("symmetric", 's');
  opt->setFlag("absolute", 'a');
  opt->setFlag("verbose", 'v');
  opt->setFlag("invert");
  opt->setFlag("dynped");

  opt->setOption("version");
  opt->setOption("nevents");
  opt->setOption("output");
  opt->setOption("calibration");
  opt->setOption("highthreshold");
  opt->setOption("lowthreshold");
  opt->setOption("symmetricwidth");
  opt->setOption("cn");
  opt->setOption("maxcn");
  opt->setOption("minstrip");
  opt->setOption("maxstrip");
  opt->setOption("side");
  opt->setOption("board");

  opt->processFile("./options.txt");
  opt->processCommandArgs(argc, argv);

  if (!opt->hasOptions())
  { /* print usage if no options */
    opt->printUsage();
    delete opt;
    return 2;
  }

  if (!opt->getValue("version"))
  {
    std::cout << "ERROR: no miniTRB version provided" << std::endl;
    return 2;
  }

  if (atoi(opt->getValue("version")) == 1212)
  {
    NChannels = 384;
    NVas = 6;
    minStrip = 0;
    maxStrip = 383;
    sensor_pitch = 0.242;
  }
  else if (atoi(opt->getValue("version")) == 1313)
  {
    NChannels = 640;
    NVas = 10;
    minStrip = 0;
    maxStrip = 639;
    sensor_pitch = 0.150;
  }
  else if (atoi(opt->getValue("version")) == 2020)
  {
    NChannels = 640;
    NVas = 10;
    minStrip = 0;
    maxStrip = 639;
    sensor_pitch = 0.150;
    newDAQ = true;
  }
  else
  {
    std::cout << "ERROR: invalid miniTRB version" << std::endl;
    return 2;
  }

  if (opt->getFlag("help") || opt->getFlag('h'))
    opt->printUsage();

  if (opt->getFlag("verbose") || opt->getFlag('v'))
    verb = true;

  if (opt->getFlag("invert"))
    invert = true;

  if (opt->getValue("highthreshold"))
    highthreshold = atof(opt->getValue("highthreshold"));

  if (opt->getValue("lowthreshold"))
    lowthreshold = atof(opt->getValue("lowthreshold"));

  if (opt->getValue("symmetric"))
    symmetric = true;

  if (opt->getValue("dynped"))
    dynped = true;

  if (opt->getValue("symmetric") && opt->getValue("symmetricwidth"))
    symmetricwidth = atoi(opt->getValue("symmetricwidth"));

  if (opt->getValue("absolute"))
    absolute = true;

  if (opt->getValue("cn"))
    cntype = atoi(opt->getValue("cn"));

  if (opt->getValue("maxcn"))
    maxCN = atoi(opt->getValue("maxcn"));

  if (opt->getValue("minstrip"))
    minStrip = atoi(opt->getValue("minstrip"));

  if (opt->getValue("maxstrip"))
    maxStrip = atoi(opt->getValue("maxstrip"));

  if (opt->getValue("side"))
    side = atoi(opt->getValue("side"));

  if (opt->getValue("board"))
    board = atoi(opt->getValue("board"));

  //////////////////Histos//////////////////
  TH1F *hADCCluster =
      new TH1F("hADCCluster", "hADCCluster", 200, 0, 200);
  hADCCluster->GetXaxis()->SetTitle("ADC");

  TH1F *hADCCluster1Strip =
      new TH1F("hADCCluster1Strip", "hADCCluster1Strip", 200, 0, 200);
  hADCCluster1Strip->GetXaxis()->SetTitle("ADC");

  TH1F *hADCCluster2Strip =
      new TH1F("hADCCluster2Strip", "hADCCluster2Strip", 200, 0, 200);
  hADCCluster2Strip->GetXaxis()->SetTitle("ADC");

  TH1F *hADCClusterManyStrip = new TH1F(
      "hADCClusterManyStrip", "hADCClusterManyStrip", 200, 0, 200);
  hADCClusterManyStrip->GetXaxis()->SetTitle("ADC");

  TH1F *hADCClusterSeed =
      new TH1F("hADCClusterSeed", "hADCClusterSeed", 200, 0, 200);
  hADCClusterSeed->GetXaxis()->SetTitle("ADC");

  TH1F *hPercentageSeed =
      new TH1F("hPercentageSeed", "hPercentageSeed", 200, 20, 200);
  hPercentageSeed->GetXaxis()->SetTitle("percentage");

  TH1F *hPercSeedintegral =
      new TH1F("hPercSeedintegral", "hPercSeedintegral", 200, 20, 200);
  hPercSeedintegral->GetXaxis()->SetTitle("percentage");

  TH1F *hClusterCharge =
      new TH1F("hClusterCharge", "hClusterCharge", 1000, -0.5, 5.5);
  hClusterCharge->GetXaxis()->SetTitle("Charge");

  TH1F *hSeedCharge = new TH1F("hSeedCharge", "hSeedCharge", 1000, -0.5, 5.5);
  hSeedCharge->GetXaxis()->SetTitle("Charge");

  TH1F *hClusterSN = new TH1F("hClusterSN", "hClusterSN", 200, 0, 250);
  hClusterSN->GetXaxis()->SetTitle("S/N");

  TH1F *hSeedSN = new TH1F("hSeedSN", "hSeedSN", 1000, 0, 200);
  hSeedSN->GetXaxis()->SetTitle("S/N");

  TH1F *hClusterCog = new TH1F("hClusterCog", "hClusterCog", (maxStrip - minStrip), minStrip - 0.5, maxStrip - 0.5);
  hClusterCog->GetXaxis()->SetTitle("cog");

  TH1F *hBeamProfile = new TH1F("hBeamProfile", "hBeamProfile", 100, -0.5, 99.5);
  hBeamProfile->GetXaxis()->SetTitle("pos (mm)");

  TH1F *hSeedPos = new TH1F("hSeedPos", "hSeedPos", (maxStrip - minStrip), minStrip - 0.5, maxStrip - 0.5);
  hSeedPos->GetXaxis()->SetTitle("strip");

  TH1F *hNclus = new TH1F("hclus", "hclus", 10, -0.5, 9.5);
  hNclus->GetXaxis()->SetTitle("n clusters");

  TH1F *hNstrip = new TH1F("hNstrip", "hNstrip", 10, -0.5, 9.5);
  hNstrip->GetXaxis()->SetTitle("n strips");

  TH1F *hNstripSeed = new TH1F("hNstripSeed", "hNstripSeed", 10, -0.5, 9.5);
  hNstripSeed->GetXaxis()->SetTitle("n strips over seed threshold");

  TH2F *hADCvsSeed = new TH2F("hADCvsSeed", "hADCvsSeed", 200, 0, 100,
                              200, 0, 100);
  hADCvsSeed->GetXaxis()->SetTitle("ADC Seed");
  hADCvsSeed->GetYaxis()->SetTitle("ADC Tot");

  TH1F *hEta = new TH1F("hEta", "hEta", 100, 0, 1);
  hEta->GetXaxis()->SetTitle("Eta");

  TH1F *hEta1 = new TH1F("hEta1", "hEta1", 100, 0, 1);
  hEta1->GetXaxis()->SetTitle("Eta (one seed)");

  TH1F *hEta2 = new TH1F("hEta2", "hEta2", 100, 0, 1);
  hEta2->GetXaxis()->SetTitle("Eta (two seed)");

  TH1F *hDifference = new TH1F("hDifference", "hDifference", 200, -5, 5);
  hDifference->GetXaxis()->SetTitle("(ADC_0-ADC_1)/(ADC_0+ADC_1)");

  TH2F *hADCvsWidth =
      new TH2F("hADCvsWidth", "hADCvsWidth", 10, -0.5, 9.5, 100, 0, 200);
  hADCvsWidth->GetXaxis()->SetTitle("# of strips");
  hADCvsWidth->GetYaxis()->SetTitle("ADC");

  TH2F *hADCvsPos = new TH2F("hADCvsPos", "hADCvsPos", (maxStrip - minStrip), minStrip - 0.5, maxStrip - 0.5,
                             1000, 0, 200);
  hADCvsPos->GetXaxis()->SetTitle("cog");
  hADCvsPos->GetYaxis()->SetTitle("ADC");

  TH2F *hADCvsEta =
      new TH2F("hADCvsEta", "hADCvsEta", 200, 0, 1, 100, 0, 200);
  hADCvsEta->GetXaxis()->SetTitle("eta");
  hADCvsEta->GetYaxis()->SetTitle("ADC");

  TH2F *hADCvsSN = new TH2F("hADCvsSN", "hADCvsSN", 200, 0, 250, 100, 0, 200);
  hADCvsSN->GetXaxis()->SetTitle("S/N");
  hADCvsSN->GetYaxis()->SetTitle("ADC");

  TH2F *hNStripvsSN =
      new TH2F("hNstripvsSN", "hNstripvsSN", 1000, 0, 500, 5, -0.5, 4.5);
  hNStripvsSN->GetXaxis()->SetTitle("S/N");
  hNStripvsSN->GetYaxis()->SetTitle("# of strips");

  TH1F *hCommonNoise0 = new TH1F("hCommonNoise0", "hCommonNoise0", 100, -20, 20);
  hCommonNoise0->GetXaxis()->SetTitle("CN");

  TH1F *hCommonNoise1 = new TH1F("hCommonNoise1", "hCommonNoise1", 100, -20, 20);
  hCommonNoise1->GetXaxis()->SetTitle("CN");

  TH1F *hCommonNoise2 = new TH1F("hCommonNoise2", "hCommonNoise2", 100, -20, 20);
  hCommonNoise2->GetXaxis()->SetTitle("CN");

  TH2F *hCommonNoiseVsVA = new TH2F("hCommonNoiseVsVA", "hCommonNoiseVsVA", 100, -20, 20, 10, -0.5, 9.5);
  hCommonNoiseVsVA->GetXaxis()->SetTitle("CN");
  hCommonNoiseVsVA->GetYaxis()->SetTitle("VA");

  TH2F *hADC0vsADC1 = new TH2F("hADC0vsADC1", "hADC0vsADC1", 100, 0, 50, 100, 0, 50);
  hADC0vsADC1->GetXaxis()->SetTitle("ADC0");
  hADC0vsADC1->GetYaxis()->SetTitle("ADC1");

  TGraph *nclus_event = new TGraph();

  // Join ROOTfiles in a single chain
  TChain *chain = new TChain();
  TChain *chain2 = new TChain();

  if (board == 0)
  {
    chain->SetName("raw_events"); //Chain input rootfiles
    for (int ii = 0; ii < opt->getArgc(); ii++)
    {
      std::cout << "Adding file " << opt->getArgv(ii) << " to the chain..." << std::endl;
      chain->Add(opt->getArgv(ii));
    }
    if (newDAQ)
    {
      chain2->SetName("raw_events_B");
      for (int ii = 0; ii < opt->getArgc(); ii++)
      {
        chain2->Add(opt->getArgv(ii));
      }
      chain->AddFriend(chain2);
    }
  }
  else if (board == 1)
  {
    chain->SetName("raw_events_C"); //Chain input rootfiles
    for (int ii = 0; ii < opt->getArgc(); ii++)
    {
      std::cout << "Adding file " << opt->getArgv(ii) << " to the chain..." << std::endl;
      chain->Add(opt->getArgv(ii));
    }
    chain2->SetName("raw_events_D");
    for (int ii = 0; ii < opt->getArgc(); ii++)
    {
      chain2->Add(opt->getArgv(ii));
    }
    chain->AddFriend(chain2);
  }
  else if (board == 2)
  {
    chain->SetName("raw_events_E"); //Chain input rootfiles
    for (int ii = 0; ii < opt->getArgc(); ii++)
    {
      std::cout << "Adding file " << opt->getArgv(ii) << " to the chain..." << std::endl;
      chain->Add(opt->getArgv(ii));
    }
    chain2->SetName("raw_events_F");
    for (int ii = 0; ii < opt->getArgc(); ii++)
    {
      chain2->Add(opt->getArgv(ii));
    }
    chain->AddFriend(chain2);
  }

  int entries = chain->GetEntries();
  if (entries == 0)
  {
    std::cout << "Error: no file or empty file" << std::endl;
    return 2;
  }
  std::cout << "This run has " << entries << " entries" << std::endl;

  if (opt->getValue("nevents"))
  {
    unsigned int temp_entries = atoi(opt->getValue("nevents"));
    if (temp_entries < entries)
    {
      entries = temp_entries;
    }
  }
  std::cout << "Processing " << entries << " entries" << std::endl;

  // Read raw event from input chain TTree
  if (side && !newDAQ)
  {
    std::cout << "Error: version selected does not contain side " << side << std::endl;
    return 2;
  }
  std::vector<unsigned int> *raw_event = 0;
  TBranch *RAW = 0;

  if (board == 0)
  {
    chain->SetBranchAddress("RAW Event", &raw_event, &RAW);

    if (side == 1)
    {
      chain->SetBranchAddress("RAW Event B", &raw_event, &RAW);
    }
  }
  else if (board == 1)
  {
    chain->SetBranchAddress("RAW Event C", &raw_event, &RAW);

    if (side == 1)
    {
      chain->SetBranchAddress("RAW Event D", &raw_event, &RAW);
    }
  }
  else if (board == 2)
  {
    chain->SetBranchAddress("RAW Event E", &raw_event, &RAW);

    if (side == 1)
    {
      chain->SetBranchAddress("RAW Event F", &raw_event, &RAW);
    }
  }

  // Create output ROOTfile
  TString output_filename;
  if (opt->getValue("output"))
  {
    output_filename = opt->getValue("output");
  }
  else
  {
    std::cout << "Error: no output file" << std::endl;
    return 2;
  }

  TFile *foutput = new TFile(output_filename + "_board_" + board + "_side_" + side + ".root", "RECREATE");
  foutput->cd();

  //Read Calibration file
  if (!opt->getValue("calibration"))
  {
    std::cout << "Error: no calibration file" << std::endl;
    return 2;
  }

  calib cal;
  read_calib(opt->getValue("calibration"), &cal);

  //histos for dynamic calibration
  TH1D *hADC[NChannels];
  TH1D *hADC_CN[NChannels];

  for (int ch = 0; ch < NChannels; ch++)
  {
    hADC[ch] = new TH1D(Form("pedestal_channel_%d_board_%d_side_%d", ch, board, side), Form("Pedestal %d", ch), 50, 0, -1);
    hADC[ch]->GetXaxis()->SetTitle("ADC");
    hADC_CN[ch] = new TH1D(Form("cn_channel_%d_board_%d_side_%d", ch, board, side), Form("CN %d", ch), 50, 0, -1);
    hADC_CN[ch]->GetXaxis()->SetTitle("ADC");
  }

  // Loop over events
  int perc = 0;
  int maxADC = 0;
  int maxEVT = 0;
  int maxPOS = 0;

  //Initialize TTree
  //TTree *clusters_tree = new TTree("cluster_tree", "cluster_tree");
  std::vector<cluster> result; //Vector of resulting clusters
  //clusters_tree->Branch("CLUSTERS", &result);
  //clusters_tree->SetAutoSave(0);

  for (int index_event = 1; index_event < entries; index_event++)
  {
    chain->GetEntry(index_event);

    if (verb)
    {
      std::cout << std::endl;
      std::cout << "EVENT: " << index_event << std::endl;
    }

    Double_t pperc = 10.0 * ((index_event + 1.0) / entries);
    if (pperc >= perc)
    {
      std::cout << "Processed " << (index_event + 1) << " out of " << entries
                << ":" << (int)(100.0 * (index_event + 1.0) / entries) << "%"
                << std::endl;
      perc++;
    }

    if ((index_event % 5000) == 0 && dynped)
    {
      std::cout << "Updating pedestals" << std::endl;

      cal = update_pedestals(hADC, NChannels, cal);
      for (int ch = 0; ch < NChannels; ch++)
      {
        hADC[ch]->Reset();
        hADC_CN[ch]->Reset();
      }
    }

    std::vector<float> signal(raw_event->size()); //Vector of pedestal subtracted signal

    if (raw_event->size() == 384 || raw_event->size() == 640)
    {
      if (cal.ped.size() >= raw_event->size())
      {
        for (size_t i = 0; i != raw_event->size(); i++)
        {
          if (cal.status[i] != 0)
          {
            signal.at(i) = 0;
          }
          else
          {
            signal.at(i) = (raw_event->at(i) - cal.ped[i]);

            if (dynped)
            {
              hADC[i]->Fill(raw_event->at(i));
            }

            if (invert)
            {
              signal.at(i) = -signal.at(i);
            }
          }
        }
      }
      else
      {
        if (verb)
        {
          std::cout << "Error: calibration file is not compatible" << std::endl;
        }
      }
    }
    else
    {
      if (verb)
      {
        std::cout << "Error: event " << index_event << " is not complete, skipping it" << std::endl;
      }
      continue;
    }

    for (int va = 0; va < NVas; va++) //Loop on VA
    {
      float cn = GetCN(&signal, va, 0);
      if (cn != -999 && abs(cn) < maxCN)
      {
        hCommonNoise0->Fill(cn);
        //hCommonNoiseVsVA->Fill(cn, va);
      }
    }

    for (int va = 0; va < NVas; va++) //Loop on VA
    {
      float cn = GetCN(&signal, va, 1);
      if (cn != -999 && abs(cn) < maxCN)
      {
        hCommonNoise1->Fill(cn);
        //hCommonNoiseVsVA->Fill(cn, va);
      }
    }

    for (int va = 0; va < NVas; va++) //Loop on VA
    {
      float cn = GetCN(&signal, va, 2);
      if (cn != -999 && abs(cn) < maxCN)
      {
        hCommonNoise2->Fill(cn);
        //hCommonNoiseVsVA->Fill(cn, va);
      }
    }

    bool goodCN = true;

    if (cntype >= 0)
    {
#pragma omp parallel for                //Multithread for loop
      for (int va = 0; va < NVas; va++) //Loop on VA
      {
        float cn = GetCN(&signal, va, cntype);
        if (cn != -999 && abs(cn) < maxCN)
        {
          hCommonNoiseVsVA->Fill(cn, va);

          for (int ch = va * 64; ch < (va + 1) * 64; ch++) //Loop on VA channels
          {
            signal.at(ch) = signal.at(ch) - cn;
          }
        }
        else
        {
          for (int ch = va * 64; ch < (va + 1) * 64; ch++)
          {
            signal.at(ch) = 0; //Invalid Common Noise Value, artificially setting VA channel to 0 signal
            goodCN = false;
          }
        }
      }
    }

    try
    {
      if (!goodCN)
        continue;

      if (*max_element(signal.begin(), signal.end()) > 4096) //4096 is the maximum ADC value possible, any more than that means the event is corrupted
        continue;

      if (*max_element(signal.begin(), signal.end()) > maxADC)
      {
        maxADC = *max_element(signal.begin(), signal.end());
        maxEVT = index_event;
        std::vector<float>::iterator it = std::find(signal.begin(), signal.end(), maxADC);
        maxPOS = std::distance(signal.begin(), it);
      }

      result = clusterize(&cal, &signal, highthreshold, lowthreshold,
                          symmetric, symmetricwidth, absolute);

      //clusters_tree->Fill();

      nclus_event->SetPoint(nclus_event->GetN(), index_event, result.size());

      for (int i = 0; i < result.size(); i++)
      {
        if (verb)
        {
          PrintCluster(result.at(i));
        }

        if (!GoodCluster(result.at(i), &cal))
          continue;

        // if ((GetClusterCOG(result.at(i)) > 205 && GetClusterCOG(result.at(i)) < 207))
        //   continue;
        //  PrintCluster(result.at(i));

        if (result.at(i).address >= minStrip && (result.at(i).address + result.at(i).width - 1) < maxStrip)
        {
          if (i == 0)
          {
            hNclus->Fill(result.size());
          }

          hADCCluster->Fill(GetClusterSignal(result.at(i)));

          if (result.at(i).width == 1)
          {
            hADCCluster1Strip->Fill(GetClusterSignal(result.at(i)));
          }
          else if (result.at(i).width == 2)
          {
            hADCCluster2Strip->Fill(GetClusterSignal(result.at(i)));
          }
          else
          {
            hADCClusterManyStrip->Fill(GetClusterSignal(result.at(i)));
          }

          hADCClusterSeed->Fill(GetClusterSeedADC(result.at(i), &cal));
          hClusterCharge->Fill(GetClusterMIPCharge(result.at(i)));
          hSeedCharge->Fill(GetSeedMIPCharge(result.at(i), &cal));
          hPercentageSeed->Fill(100 * GetClusterSeedADC(result.at(i), &cal) / GetClusterSignal(result.at(i)));
          hClusterSN->Fill(GetClusterSN(result.at(i), &cal));
          hSeedSN->Fill(GetSeedSN(result.at(i), &cal));

          if (verb)
          {
            std::cout << "Adding cluster with COG: " << GetClusterCOG(result.at(i)) << std::endl;
          }

          hClusterCog->Fill(GetClusterCOG(result.at(i)));
          hBeamProfile->Fill(GetPosition(result.at(i), sensor_pitch));
          hSeedPos->Fill(GetClusterSeed(result.at(i), &cal));
          hNstrip->Fill(GetClusterWidth(result.at(i)));
          if (result.at(i).width == 2)
          {
            hEta->Fill(GetClusterEta(result.at(i)));
            if (result.at(i).over == 1)
            {
              hEta1->Fill(GetClusterEta(result.at(i)));
            }
            else
            {
              hEta2->Fill(GetClusterEta(result.at(i)));
            }
            hADCvsEta->Fill(GetClusterEta(result.at(i)), GetClusterSignal(result.at(i)));
          }
          hADCvsWidth->Fill(GetClusterWidth(result.at(i)), GetClusterSignal(result.at(i)));
          hADCvsPos->Fill(GetClusterCOG(result.at(i)), GetClusterSignal(result.at(i)));
          hADCvsSeed->Fill(GetClusterSeedADC(result.at(i), &cal), GetClusterSignal(result.at(i)));
          hADCvsSN->Fill(GetClusterSN(result.at(i), &cal), GetClusterSignal(result.at(i)));
          hNStripvsSN->Fill(GetClusterSN(result.at(i), &cal), GetClusterWidth(result.at(i)));
          hNstripSeed->Fill(result.at(i).over);

          if (result.at(i).width == 2)
          {
            hDifference->Fill((result.at(i).ADC.at(0) - result.at(i).ADC.at(1)) / (result.at(i).ADC.at(0) + result.at(i).ADC.at(1)));
            hADC0vsADC1->Fill(result.at(i).ADC.at(0), result.at(i).ADC.at(1));
          }
        }
      }
    }
    catch (const char *msg)
    {
      if (verb)
      {
        std::cerr << msg << "Skipping event " << index_event << std::endl;
      }
      hNclus->Fill(0);
      continue;
    }
  }

  if (verb)
  {
    std::cout << "Maximum ADC value found is " << maxADC
              << " in event number " << maxEVT
              << " at strip " << maxPOS << std::endl;
  }
  hNclus->Write();

  Double_t norm = hADCCluster->GetEntries();
  //hADCCluster->Scale(1 / norm);
  hADCCluster->Write();

  Double_t norm1 = hADCCluster1Strip->GetEntries();
  //hADCCluster1Strip->Scale(1 / norm1);
  hADCCluster1Strip->Write();

  Double_t norm2 = hADCCluster2Strip->GetEntries();
  //hADCCluster2Strip->Scale(1 / norm2);
  hADCCluster2Strip->Write();

  Double_t norm3 = hADCClusterManyStrip->GetEntries();
  //hADCClusterManyStrip->Scale(1 / norm3);
  hADCClusterManyStrip->Write();

  hADCClusterSeed->Write();
  hClusterCharge->Write();
  hSeedCharge->Write();
  hClusterSN->Write();
  hSeedSN->Write();
  hClusterCog->Write();
  hBeamProfile->Write();
  hSeedPos->Write();

  hNstrip->Write();

  hNstripSeed->Write();
  hEta->Write();
  hEta1->Write();
  hEta2->Write();
  hADCvsWidth->Write();
  hADCvsPos->Write();
  hADCvsSeed->Write();
  hADCvsEta->Write();
  hADCvsSN->Write();
  hNStripvsSN->Write();
  hDifference->Write();
  hADC0vsADC1->Write();
  hCommonNoise0->Write();
  hCommonNoise1->Write();
  hCommonNoise2->Write();
  hCommonNoiseVsVA->Write();

  nclus_event->SetTitle("nClus vs nEvent");
  nclus_event->GetXaxis()->SetTitle("# event");
  nclus_event->GetYaxis()->SetTitle("# clusters");
  nclus_event->SetMarkerColor(kRed + 1);
  nclus_event->SetLineColor(kRed + 1);
  nclus_event->SetMarkerSize(0.5);
  nclus_event->Draw("*lSAME");
  nclus_event->Write();

  //clusters_tree->Write();

  foutput->Close();
  return 0;
}
