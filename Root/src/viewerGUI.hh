#include <TQObject.h>
#include <RQ_OBJECT.h>
#include "TGraph.h"

class TGWindow;
class TGMainFrame;
class TRootEmbeddedCanvas;
class TGNumberEntry;
class TGHorizontalFrame;
class TGVerticalFrame;
class TGTextButton;
class TGLabel;
class TGTextView;
class TGLayout;
class TGFrame;
class TGFileDialog;
class TGCheckButton;

class MyMainFrame
{
  RQ_OBJECT("MyMainFrame")
private:
  TGMainFrame *fMain;
  TRootEmbeddedCanvas *fEcanvas;
  TGNumberEntry *fNumber, *fNumber1;
  TGHorizontalFrame *fHor0, *fHor0b, *fHor1, *fHor3, *fHor4;
  TGVerticalFrame *fVer0, *fVer1;
  TGTextButton *fExit, *fDraw, *fOpen, *fSave;
  TGLabel *evtLabel, *fileLabel, *pedLabel, *detectorLabel;
  TGLabel *calibLabel, *calibLabel2, *calibLabel3, *calibLabel4, *calibLabel5, *calibLabel6; 
  TGLabel *calibLabel7, *calibLabel8, *calibLabel9, *calibLabel10, *calibLabel11, *calibLabel12;
  TGLabel *calibLabel13, *calibLabel14, *calibLabel15, *calibLabel16;
  TGTextView *fStatusBar;
  TGCheckButton *fPed;
  TGraph *gr_event = new TGraph();
  bool newDAQ = false;
  int boards = 1;

public:
  MyMainFrame(const TGWindow *p, UInt_t w, UInt_t h);
  virtual ~MyMainFrame();
  void DoDraw();
  void DoOpen();
  void DoClose();
  void DoOpenCalib(bool newDAQ, int boards);
  void PrintCode(Int_t code);
  void viewer(int evt, int detector, char filename[200], char calibfile[200], int boards);
  ClassDef(MyMainFrame, 0)
};
