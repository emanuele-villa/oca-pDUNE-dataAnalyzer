#include "TFile.h"
#include "TChain.h"
#include "TGraph.h"
#include "TLine.h"
#include "TROOT.h"
#include <TApplication.h>
#include <TCanvas.h>
#include <TColor.h>
#include <TString.h>
#include <TGCanvas.h>
#include <TGWindow.h>
#include <TGFrame.h>
#include <TRootEmbeddedCanvas.h>
#include <TGNumberEntry.h>
#include <TGLabel.h>
#include <TGTextView.h>
#include <TGLayout.h>
#include <TGFrame.h>
#include <TGFileDialog.h>
#include <TGClient.h>
#include "TH1.h"
#include <TGMsgBox.h>

#include "viewerGUI.hh"
#include "event.h"

#include <iostream>
#include <fstream>
#include <string>

MyMainFrame::MyMainFrame(const TGWindow *p, UInt_t w, UInt_t h)
{
  newDAQ = false;
  boards = 1;
  // Create a main frame
  fMain = new TGMainFrame(p, w, h);
  fMain->Connect("CloseWindow()", "MyMainFrame", this, "DoClose()");
  fMain->DontCallClose();

  // Create canvas widget
  fEcanvas = new TRootEmbeddedCanvas("Ecanvas", fMain, 1024, 500);
  fMain->AddFrame(fEcanvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 10, 10, 10, 1));

  fHor0 = new TGHorizontalFrame(fMain, 1024, 20);
  fHor0b = new TGHorizontalFrame(fMain, 1024, 20);

  TGVerticalFrame *fVer0 = new TGVerticalFrame(fHor0b, 10, 10);
  TGVerticalFrame *fVer1 = new TGVerticalFrame(fHor0b, 10, 10);

  fStatusBar = new TGTextView(fVer1, 500, 150);
  fStatusBar->LoadBuffer("Event viewer for DAMPE/FOOT raw data .root files.");
  fStatusBar->AddLine("");
  fStatusBar->AddLine("Root files must have been processed by miniTRB_compress or FOOT_compress utilities");
  fStatusBar->AddLine("");
  fStatusBar->AddLine("Files must have been acquired in raw (non compressed) mode.");
  fStatusBar->AddLine("");

  fHor1 = new TGHorizontalFrame(fVer0, 1024, 20);
  fHor3 = new TGHorizontalFrame(fMain, 1024, 20);
  fHor4 = new TGHorizontalFrame(fVer0, 1024, 20);

  fOpen = new TGTextButton(fHor0, "&Open");
  fOpen->Connect("Clicked()", "MyMainFrame", this, "DoOpen()");

  fDraw = new TGTextButton(fHor0, "&Draw");
  fDraw->Connect("Clicked()", "MyMainFrame", this, "DoDraw()");

  //fExit = new TGTextButton(fHor0, "&Exit", "gApplication->Terminate(0)");
  fExit = new TGTextButton(fHor0, "&Exit");
  fExit->Connect("Clicked()", "MyMainFrame", this, "DoClose()");

  evtLabel = new TGLabel(fHor1, "Event Number:");
  fHor1->AddFrame(evtLabel, new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 5, 2, 2, 2));
  fNumber = new TGNumberEntry(fHor1, 0, 10, -1, TGNumberFormat::kNESReal, TGNumberFormat::kNEANonNegative, TGNumberFormat::kNELLimitMax, 0, 1);
  fNumber->GetNumberEntry()->Connect("ReturnPressed()", "MyMainFrame", this, "DoDraw()");
  fHor1->AddFrame(fNumber, new TGLayoutHints(kLHintsCenterX, 5, 5, 5, 5));

  detectorLabel = new TGLabel(fHor1, "Sensor number:");
  fHor1->AddFrame(detectorLabel, new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 5, 2, 2, 2));
  fNumber1 = new TGNumberEntry(fHor1, 0, 10, -1, TGNumberFormat::kNESReal, TGNumberFormat::kNEANonNegative, TGNumberFormat::kNELLimitMax, 0, 6);
  fNumber1->GetNumberEntry()->Connect("ReturnPressed()", "MyMainFrame", this, "DoDraw()");
  fHor1->AddFrame(fNumber1, new TGLayoutHints(kLHintsCenterX, 5, 5, 5, 5));

  fPed = new TGCheckButton(fHor4, "Pedestal subtraction");
  //fPed->SetOn();
  fHor4->AddFrame(fPed, new TGLayoutHints(kLHintsExpandX | kLHintsLeft | kLHintsCenterY, 5, 2, 2, 2));

  fileLabel = new TGLabel(fHor3, "No rootfile opened");
  calibLabel = new TGLabel(fHor3, "No calibfile opened");
  calibLabel2 = new TGLabel(fHor3, "No calibfile opened");
  calibLabel3 = new TGLabel(fHor3, "No calibfile opened");
  calibLabel4 = new TGLabel(fHor3, "No calibfile opened");
  calibLabel5 = new TGLabel(fHor3, "No calibfile opened");
  calibLabel6 = new TGLabel(fHor3, "No calibfile opened");
  calibLabel6 = new TGLabel(fHor3, "No calibfile opened");
  calibLabel7 = new TGLabel(fHor3, "No calibfile opened");
  calibLabel8 = new TGLabel(fHor3, "No calibfile opened");
  calibLabel9 = new TGLabel(fHor3, "No calibfile opened");
  calibLabel10 = new TGLabel(fHor3, "No calibfile opened");
  calibLabel11 = new TGLabel(fHor3, "No calibfile opened");
  calibLabel12 = new TGLabel(fHor3, "No calibfile opened");

  TColor *color = gROOT->GetColor(26);
  color->SetRGB(0.91, 0.91, 0.91);
  calibLabel->SetTextColor(color);
  calibLabel2->SetTextColor(color);
  calibLabel3->SetTextColor(color);
  calibLabel4->SetTextColor(color);
  calibLabel5->SetTextColor(color);
  calibLabel6->SetTextColor(color);
  calibLabel7->SetTextColor(color);
  calibLabel8->SetTextColor(color);
  calibLabel9->SetTextColor(color);
  calibLabel10->SetTextColor(color);
  calibLabel11->SetTextColor(color);
  calibLabel12->SetTextColor(color);

  fHor3->AddFrame(calibLabel, new TGLayoutHints(kLHintsExpandX | kLHintsLeft | kLHintsCenterY, 5, 2, 2, 2));
  fHor3->AddFrame(fileLabel, new TGLayoutHints(kLHintsExpandX | kLHintsLeft | kLHintsCenterY, 5, 2, 2, 2));
  fHor3->AddFrame(calibLabel2, new TGLayoutHints(kLHintsExpandX | kLHintsLeft | kLHintsCenterY, 5, 2, 2, 2));

  fHor4->AddFrame(fPed, new TGLayoutHints(kLHintsCenterX, 5, 5, 5, 5));

  fHor0->AddFrame(fOpen, new TGLayoutHints(kLHintsCenterX, 5, 5, 3, 4));
  fHor0->AddFrame(fDraw, new TGLayoutHints(kLHintsCenterX, 5, 5, 3, 4));
  fHor0->AddFrame(fExit, new TGLayoutHints(kLHintsCenterX, 5, 5, 3, 4));

  fHor0b->AddFrame(fVer0, new TGLayoutHints(kLHintsCenterX | kLHintsExpandX | kLHintsExpandY, 2, 2, 5, 1));
  fHor0b->AddFrame(fVer1, new TGLayoutHints(kLHintsCenterX | kLHintsExpandX | kLHintsExpandY, 2, 2, 5, 1));

  fMain->AddFrame(fHor0, new TGLayoutHints(kLHintsCenterX, 2, 2, 5, 1));
  fMain->AddFrame(fHor0b, new TGLayoutHints(kLHintsExpandX | kLHintsCenterX, 2, 2, 5, 1));

  fVer0->AddFrame(fHor1, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY, 2, 2, 5, 1));
  fVer0->AddFrame(fHor4, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY, 2, 2, 5, 1));
  fMain->AddFrame(fHor3, new TGLayoutHints(kLHintsExpandX | kLHintsCenterX, 2, 2, 5, 1));

  fVer1->AddFrame(fStatusBar, new TGLayoutHints(kLHintsCenterX | kLHintsBottom | kLHintsLeft | kLHintsExpandY, 5, 5, 2, 2));

  fMain->SetCleanup(kDeepCleanup);
  fMain->SetWindowName("DAMPE/FOOT Raw Event Viewer");
  fMain->MapSubwindows();
  fMain->Resize(fMain->GetDefaultSize());
  fMain->MapWindow();
}

const char *filetypesROOT[] = {
    "ROOT files", "*.root",
    "All files", "*",
    0, 0};

const char *filetypesCalib[] = {
    "Calib files", "*.cal",
    "All files", "*",
    0, 0};

void MyMainFrame::viewer(int evt, int detector, char filename[200], char calibfile[200], int boards)
{
  TChain *chain = new TChain("raw_events");
  TChain *chain2 = new TChain("raw_events_B");
  TChain *chain3 = new TChain("raw_events_C");
  TChain *chain4 = new TChain("raw_events_D");
  TChain *chain5 = new TChain("raw_events_E");
  TChain *chain6 = new TChain("raw_events_F");
  TChain *chain7 = new TChain("raw_events_G");
  TChain *chain8 = new TChain("raw_events_H");
  TChain *chain9 = new TChain("raw_events_I");
  TChain *chain10 = new TChain("raw_events_J");
  TChain *chain11 = new TChain("raw_events_K");
  TChain *chain12 = new TChain("raw_events_L");

  chain->Add(filename);
  if (newDAQ)
  {
    chain2->Add(filename);
    chain->AddFriend(chain2);

    if (boards >= 2)
    {
      chain3->Add(filename);
      chain4->Add(filename);
      chain->AddFriend(chain3);
      chain->AddFriend(chain4);
    }

    if (boards >= 3)
    {
      chain5->Add(filename);
      chain6->Add(filename);
      chain->AddFriend(chain5);
      chain->AddFriend(chain6);
    }

    if (boards >= 4)
    {
      chain7->Add(filename);
      chain8->Add(filename);
      chain->AddFriend(chain7);
      chain->AddFriend(chain8);
    }

    if (boards >= 5)
    {
      chain9->Add(filename);
      chain10->Add(filename);
      chain->AddFriend(chain9);
      chain->AddFriend(chain10);
    }

    if (boards >= 6)
    {
      chain11->Add(filename);
      chain12->Add(filename);
      chain->AddFriend(chain11);
      chain->AddFriend(chain12);
    }
  }
  Long64_t entries = chain->GetEntries();

  fStatusBar->AddLine("");
  fStatusBar->AddLine("Event: " + TGString(evt) + " of: " + TGString(entries - 1) + " for detector: " + TGString(detector));
  fStatusBar->ShowBottom();

  //Read Calibration file
  calib cal;
  read_calib(calibfile, &cal);

  std::string alphabet = "ABCDEFGHIJKLMNOPQRSTWXYZ";
  // Read raw event from input chain TTree
  std::vector<unsigned int> *raw_event = 0;
  TBranch *RAW = 0;

  if(detector == 0)
  {
    chain->SetBranchAddress("RAW Event", &raw_event, &RAW);
  } 
  else
  {
    chain->SetBranchAddress((TString)"RAW Event "+alphabet.at(detector), &raw_event, &RAW);
  }

  gr_event->SetMarkerColor(kRed + 1);
  gr_event->SetLineColor(kRed + 1);

  gr_event->SetMarkerStyle(23);
  gr_event->GetXaxis()->SetNdivisions(raw_event->size() / 64, false);

  if (newDAQ)
  {
    fStatusBar->AddLine("Opened FOOT raw file from DE10nano");
  }
  else
  {
    fStatusBar->AddLine("Opened DaMPE/FOOT raw file from miniTRB");
  }

  int maxadc = -999;
  int minadc = 0;

  chain->GetEntry(evt);

  gr_event->Set(0);

  for (int chan = 0; chan < raw_event->size(); chan++)
  {
    double ADC = raw_event->at(chan);
    double signal;

    if (fPed->IsOn())
    {
      signal = ADC - cal.ped[chan];
    }
    else
    {
      signal = ADC;
    }

    if (signal > maxadc)
      maxadc = signal;
    if (signal < minadc)
      minadc = signal;

    gr_event->SetPoint(gr_event->GetN(), chan, signal);
  }

  TH1F *frame = gPad->DrawFrame(0, minadc - 20, raw_event->size(), maxadc + 20);

  int nVAs = raw_event->size() / 64;

  frame->SetTitle("Event number " + TString::Format("%0d", (int)evt) + " Side: " + TString::Format("%0d", (int)detector));
  frame->GetXaxis()->SetNdivisions(-nVAs);
  frame->GetXaxis()->SetTitle("Strip number");
  frame->GetYaxis()->SetTitle("ADC");

  gr_event->SetMarkerSize(0.5);
  gr_event->Draw("*lSAME");
  gr_event->Draw();
  TCanvas *fCanvas = fEcanvas->GetCanvas();
  fCanvas->SetGrid();
  fCanvas->cd();
  fCanvas->Update();
  delete chain;
  delete chain2;
}

void MyMainFrame::DoDraw()
{
  if (gROOT->GetListOfFiles()->FindObject((char *)(fileLabel->GetText())->GetString()))
  {
    if (fNumber1->GetNumberEntry()->GetIntNumber() == 0)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel->GetText())->GetString(), boards);
    }
    else if (fNumber1->GetNumberEntry()->GetIntNumber() == 1)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel2->GetText())->GetString(), boards);
    }
    else if (fNumber1->GetNumberEntry()->GetIntNumber() == 2)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel3->GetText())->GetString(), boards);
    }
    else if (fNumber1->GetNumberEntry()->GetIntNumber() == 3)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel4->GetText())->GetString(), boards);
    }
    else if (fNumber1->GetNumberEntry()->GetIntNumber() == 4)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel5->GetText())->GetString(), boards);
    }
    else if (fNumber1->GetNumberEntry()->GetIntNumber() == 5)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel6->GetText())->GetString(), boards);
    }
    else if (fNumber1->GetNumberEntry()->GetIntNumber() == 6)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel7->GetText())->GetString(), boards);
    }
    else if (fNumber1->GetNumberEntry()->GetIntNumber() == 7)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel8->GetText())->GetString(), boards);
    }
    else if (fNumber1->GetNumberEntry()->GetIntNumber() == 8)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel9->GetText())->GetString(), boards);
    }
    else if (fNumber1->GetNumberEntry()->GetIntNumber() == 9)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel10->GetText())->GetString(), boards);
    }
    else if (fNumber1->GetNumberEntry()->GetIntNumber() == 10)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel11->GetText())->GetString(), boards);
    }
    else if (fNumber1->GetNumberEntry()->GetIntNumber() == 11)
    {
      viewer(fNumber->GetNumberEntry()->GetIntNumber(), fNumber1->GetNumberEntry()->GetIntNumber(), (char *)(fileLabel->GetText())->GetString(), (char *)(calibLabel12->GetText())->GetString(), boards);
    }
  }
}

void MyMainFrame::DoOpen()
{
  static TString dir(".");
  TGFileInfo fi;
  fi.fFileTypes = filetypesROOT;
  fi.fIniDir = StrDup(dir);
  new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
  fNumber1->SetState(false);

  if (fi.fFilename)
  {
    TFile *f = TFile::Open(fi.fFilename);
    bool IStree = f->GetListOfKeys()->Contains("raw_events");

    bool ISfoot = f->GetListOfKeys()->Contains("raw_events_B");
    if (ISfoot)
    {
      fNumber1->SetState(true);
      fNumber1->SetLimitValues(0, 1);
      newDAQ = true;
      if (f->GetListOfKeys()->Contains("raw_events_C"))
      {
        fNumber1->SetLimitValues(0, 3);
        boards = 2;
      }
      if (f->GetListOfKeys()->Contains("raw_events_E"))
      {
        fNumber1->SetLimitValues(0, 5);
        boards = 3;
      }
      if (f->GetListOfKeys()->Contains("raw_events_G"))
      {
        fNumber1->SetLimitValues(0, 7);
        boards = 4;
      }
      if (f->GetListOfKeys()->Contains("raw_events_I"))
      {
        fNumber1->SetLimitValues(0, 9);
        boards = 5;
      }
      if (f->GetListOfKeys()->Contains("raw_events_K"))
      {
        fNumber1->SetLimitValues(0, 11);
        boards = 6;
      }
    }

    if (IStree)
    {
      TTree *t = (TTree *)f->Get("raw_events");
      int entries = t->GetEntries();
      fNumber->SetLimitValues(0, entries - 1);
      fStatusBar->Clear();
      fileLabel->SetText(fi.fFilename);
      fNumber->SetText("0");

      Int_t buttons = kMBYes + kMBNo;
      Int_t retval;

      new TGMsgBox(gClient->GetRoot(), fMain,
                   "Calib?", "Do you want to load the calibration files?",
                   kMBIconQuestion, buttons, &retval);
      if (retval == kMBYes)
      {
        fPed->SetOn();
        DoOpenCalib(newDAQ, boards);
      }
      else
      {
        fPed->SetDisabledAndSelected(0);
      }
    }
    else
    {
      fStatusBar->Clear();
      fStatusBar->AddLine("ERROR: the file does not contain raw events TTree");
      return;
    }
  }
  dir = fi.fIniDir;
}

void MyMainFrame::DoOpenCalib(bool newDAQ, int boards)
{
  static TString dir(".");
  TGFileInfo fi;
  fi.fFileTypes = filetypesCalib;
  fi.fIniDir = StrDup(dir);
  new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);

  if (fi.fFilename)
  {
    calibLabel->SetText(fi.fFilename);
    fStatusBar->LoadBuffer("Run: " + TGString(fileLabel->GetText()->GetString()));
    fStatusBar->AddLine("");
    fStatusBar->AddLine("Calibration 1: " + TGString(calibLabel->GetText()->GetString()));
    //DoDraw();
  }
  else
  {
    //fStatusBar->Clear();
    fStatusBar->AddLine("ERROR: calibration file is empty");
    return;
  }

  dir = fi.fIniDir;

  if (newDAQ)
  {
    new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
    if (fi.fFilename)
    {
      calibLabel2->SetText(fi.fFilename);
      fStatusBar->AddLine("Calibration 2: " + TGString(calibLabel2->GetText()->GetString()));
    }
    else
    {
      //fStatusBar->Clear();
      fStatusBar->AddLine("ERROR: calibration file 2 is empty");
      return;
    }

    if (boards >= 2)
    {
      new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
      if (fi.fFilename)
      {
        calibLabel3->SetText(fi.fFilename);
        fStatusBar->AddLine("Calibration 3: " + TGString(calibLabel3->GetText()->GetString()));
      }
      else
      {
        //fStatusBar->Clear();
        fStatusBar->AddLine("ERROR: calibration file 3 is empty");
        return;
      }

      new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
      if (fi.fFilename)
      {
        calibLabel4->SetText(fi.fFilename);
        fStatusBar->AddLine("Calibration 4: " + TGString(calibLabel4->GetText()->GetString()));
      }
      else
      {
        //fStatusBar->Clear();
        fStatusBar->AddLine("ERROR: calibration file 4 is empty");
        return;
      }
    }

    if (boards >= 3)
    {
      new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
      if (fi.fFilename)
      {
        calibLabel5->SetText(fi.fFilename);
        fStatusBar->AddLine("Calibration 5: " + TGString(calibLabel5->GetText()->GetString()));
      }
      else
      {
        //fStatusBar->Clear();
        fStatusBar->AddLine("ERROR: calibration file 5 is empty");
        return;
      }

      new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
      if (fi.fFilename)
      {
        calibLabel6->SetText(fi.fFilename);
        fStatusBar->AddLine("Calibration 6: " + TGString(calibLabel6->GetText()->GetString()));
      }
      
      else
      {
        //fStatusBar->Clear();
        fStatusBar->AddLine("ERROR: calibration file 6 is empty");
        return;
      }
    }

    if (boards >= 4)
    {
      new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
      if (fi.fFilename)
      {
        calibLabel7->SetText(fi.fFilename);
        fStatusBar->AddLine("Calibration 7: " + TGString(calibLabel7->GetText()->GetString()));
      }
      else
      {
        //fStatusBar->Clear();
        fStatusBar->AddLine("ERROR: calibration file 7 is empty");
        return;
      }

      new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
      if (fi.fFilename)
      {
        calibLabel8->SetText(fi.fFilename);
        fStatusBar->AddLine("Calibration 8: " + TGString(calibLabel8->GetText()->GetString()));
      }
      
      else
      {
        //fStatusBar->Clear();
        fStatusBar->AddLine("ERROR: calibration file 8 is empty");
        return;
      }
    }

    if (boards >= 5)
    {
      new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
      if (fi.fFilename)
      {
        calibLabel9->SetText(fi.fFilename);
        fStatusBar->AddLine("Calibration 9: " + TGString(calibLabel9->GetText()->GetString()));
      }
      else
      {
        //fStatusBar->Clear();
        fStatusBar->AddLine("ERROR: calibration file 9 is empty");
        return;
      }

      new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
      if (fi.fFilename)
      {
        calibLabel10->SetText(fi.fFilename);
        fStatusBar->AddLine("Calibration 10: " + TGString(calibLabel10->GetText()->GetString()));
      }
      
      else
      {
        //fStatusBar->Clear();
        fStatusBar->AddLine("ERROR: calibration file 10 is empty");
        return;
      }
    }

    if (boards >= 6)
    {
      new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
      if (fi.fFilename)
      {
        calibLabel11->SetText(fi.fFilename);
        fStatusBar->AddLine("Calibration 11: " + TGString(calibLabel11->GetText()->GetString()));
      }
      else
      {
        //fStatusBar->Clear();
        fStatusBar->AddLine("ERROR: calibration file 11 is empty");
        return;
      }

      new TGFileDialog(gClient->GetRoot(), fMain, kFDOpen, &fi);
      if (fi.fFilename)
      {
        calibLabel12->SetText(fi.fFilename);
        fStatusBar->AddLine("Calibration 12: " + TGString(calibLabel12->GetText()->GetString()));
      }
      
      else
      {
        //fStatusBar->Clear();
        fStatusBar->AddLine("ERROR: calibration file 12 is empty");
        return;
      }
    }
  }
}

void MyMainFrame::DoClose()
{
  //
  Int_t buttons = kMBYes + kMBNo;
  Int_t retval;

  new TGMsgBox(gClient->GetRoot(), fMain,
               "Exit", "Are you sure you want to exit?",
               kMBIconQuestion, buttons, &retval);
  if (retval == kMBYes)
  {
    gApplication->Terminate(0);
  }
}

MyMainFrame::~MyMainFrame()
{
  // Clean up used widgets: frames, buttons, layout hints
  fMain->Cleanup();
  delete fMain;
}

void viewerGUI()
{
  // Popup the GUI...
  new MyMainFrame(gClient->GetRoot(), 1000, 1000);
}

int main(int argc, char **argv)
{
  TApplication theApp("App", &argc, argv);
  viewerGUI();
  theApp.Run();
  return 0;
}
