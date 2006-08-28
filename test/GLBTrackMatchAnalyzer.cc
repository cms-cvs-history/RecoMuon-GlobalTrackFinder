// -*- C++ -*-
//
// Package:    GLBTrackMatchAnalyzer
// Class:      GLBTrackMatchAnalyzer
// 
/**

 Description: <one line class summary>

 Implementation:
     <Notes on implementation>
*/
//
// Original Author:  Adam Everett
//         Created:  Thu Jul 20 23:15:21 CEST 2006
// $Id: GLBTrackMatchAnalyzer.cc,v 1.3 2006/08/04 15:02:08 aeverett Exp $
//
//


// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDAnalyzer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"

#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "RecoMuon/GlobalTrackFinder/interface/GlobalMuonTrackMatcher.h"
#include "RecoMuon/TrackingTools/interface/MuonUpdatorAtVertex.h"

#include "TrackingTools/TransientTrack/interface/TransientTrack.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateOnSurface.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "TFile.h"
#include "TH1F.h"

using namespace std;
using namespace edm;

//
// class decleration
//

class GLBTrackMatchAnalyzer : public edm::EDAnalyzer {
public:
  explicit GLBTrackMatchAnalyzer(const edm::ParameterSet&);
  ~GLBTrackMatchAnalyzer();
   
  virtual void analyze(const edm::Event&, const edm::EventSetup&);
private:
  // ----------member data ---------------------------
  std::string theRootFileName;
  TFile* theFile;
  
  GlobalMuonTrackMatcher* theTrackMatcher;
  edm::ESHandle<MagneticField> theMGField;
  
  std::string theSTAMuonLabel;
  std::string theTkTracksLabel;
  std::string theDataType;
  
  double theTrackMatcherChi2Cut;
  
  // Histograms
  TH1F *hChi2;
  
  // Counters
  int numberOfTkTracks;
  int numberOfStaTracks;
  
};

//
// constants, enums and typedefs
//

//
// static data member definitions
//

//
// constructors and destructor
//
GLBTrackMatchAnalyzer::GLBTrackMatchAnalyzer(const edm::ParameterSet& iConfig)
{
  numberOfStaTracks = 0;
  numberOfTkTracks = 0;
  //now do what ever initialization is needed
  theSTAMuonLabel = iConfig.getUntrackedParameter<string>("MuonCollectionLabel");
  theTkTracksLabel = iConfig.getUntrackedParameter<string>("TkTrackCollectionLabel");
  theRootFileName = iConfig.getUntrackedParameter<string>("rootFileName");
  theDataType = iConfig.getUntrackedParameter<string>("DataType");
  theTrackMatcherChi2Cut = iConfig.getParameter<double>("Chi2CutTrackMatcher");
  
  theTrackMatcher = new GlobalMuonTrackMatcher(theTrackMatcherChi2Cut);
  
  if(theDataType != "RealData" && theDataType != "SimData")
    cout<<"Error in Data Type!!"<<endl;
  
  // Create the root file
  theFile = new TFile(theRootFileName.c_str(), "RECREATE");
  theFile->cd();
  
  hChi2 = new TH1F("trackChi2","Track Chi2",10000,0,100000);
  
}


GLBTrackMatchAnalyzer::~GLBTrackMatchAnalyzer()
{
  
  // do anything here that needs to be done at desctruction time
  // (e.g. close files, deallocate resources etc.)
  
  if(theDataType == "SimData"){
    cout << endl << endl << "Number of Events with STA tracks: " << numberOfStaTracks << endl;
  }
  
  cout << "Number of Events with Tk tracks: " << numberOfTkTracks << endl << endl;
  cout << "Number of histogram entries: " << hChi2->GetEntries() << endl << endl;
  // Write the histos to file
  theFile->cd();
  hChi2->Write();
  theFile->Close();
  
}


//
// member functions
//

// ------------ method called to produce the data  ------------
void
GLBTrackMatchAnalyzer::analyze(const edm::Event& event, 
			       const edm::EventSetup& eventSetup)
{
  //using namespace edm;
  LogInfo("TrackMatchTestAnalyzer")
    << "***** Run: " << event.id().run() 
    << " Event: " << event.id().event() << endl;
  
  // Get the RecTrack collection from the event
  edm::Handle<reco::TrackCollection> staTracks;
  event.getByLabel(theSTAMuonLabel, staTracks);
  
  edm::Handle<reco::TrackCollection> tkTracks;
  event.getByLabel(theTkTracksLabel, tkTracks);
  
  eventSetup.get<IdealMagneticFieldRecord>().get(theMGField);

  theTrackMatcher->setES(eventSetup);

  LogInfo("GLBTrackMatchAnalyzer")
    <<"Reconstructed STAMuon tracks: " 
    << staTracks->size() << "\n";
  LogInfo("GLBTrackMatchAnalyzer")
    <<"Reconstructed Tracker tracks: " 
    << tkTracks->size() << "\n";
  if(tkTracks->size() >= 1) {numberOfTkTracks++;}
  if(staTracks->size() >= 1) {numberOfStaTracks++;}
  for (unsigned int position = 0; position != staTracks->size(); ++position) {
    //LogInfo("GLBTrackMatchAnalyzer")
    //<<"+++ New Track +++ "<< position <<endl;
    
    reco::TrackRef staTrack(staTracks,position);
    
    ////LogInfo("GLBTrackMatchAnalyzer")
    ////<<"staTrack "<< staTrack->momentum() <<endl;
    //std::pair<bool, reco::TrackRef> bestMatch = 
    //theTrackMatcher->matchOne(staTrack,tkTracks);
    
    //if(bestMatch.first){
    //std::pair<bool, double> bestChi2 = 
    //theTrackMatcher->match(staTrack,bestMatch.second);
    //hChi2->Fill(bestChi2.second);
    //}
  }  
}

//define this as a plug-in
DEFINE_FWK_MODULE(GLBTrackMatchAnalyzer)
  
