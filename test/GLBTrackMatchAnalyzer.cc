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
// $Id$
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
#include "RecoMuon/TrackingTools/interface/MuonPatternRecoDumper.h"

#include "RecoMuon/GlobalTrackFinder/interface/GlobalMuonTrackMatcher.h"

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

  std::string theSTAMuonLabel;
  std::string theTkTracksLabel;
  std::string theSeedCollectionLabel;
  std::string theDataType;

  double theTrackMatcherChi2Cut;

  // Histograms
  TH1F *hChi2;

  // Counters
  int numberOfSimTracks;
  int numberOfRecTracks;

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
   //now do what ever initialization is needed
  
  theSTAMuonLabel = iConfig.getUntrackedParameter<string>("StandAloneTrackCollectionLabel");
  theTkTracksLabel = iConfig.getUntrackedParameter<string>("TkTracksCollectionLabel");
  theSeedCollectionLabel = iConfig.getUntrackedParameter<string>("MuonSeedCollectionLabel");

  theRootFileName = iConfig.getUntrackedParameter<string>("rootFileName");

  theDataType = iConfig.getUntrackedParameter<string>("DataType");
  theTrackMatcherChi2Cut = iConfig.getParameter<double>("Chi2CutTrackMatcher");
  
  if(theDataType != "RealData" && theDataType != "SimData")
    cout<<"Error in Data Type!!"<<endl;

  // Create the root file
  theFile = new TFile(theRootFileName.c_str(), "RECREATE");
  theFile->cd();

  hChi2 = new TH1F("trackChi2","Track Chi2",100,-2,2);
  
}


GLBTrackMatchAnalyzer::~GLBTrackMatchAnalyzer()
{
 
   // do anything here that needs to be done at desctruction time
   // (e.g. close files, deallocate resources etc.)
  
  if(theDataType == "SimData"){
    cout << endl << endl << "Number of Sim tracks: " << numberOfSimTracks << endl;
  }

  cout << "Number of Reco tracks: " << numberOfRecTracks << endl << endl;
    
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
GLBTrackMatchAnalyzer::analyze(const edm::Event& event, const edm::EventSetup& eventSetup)
{
  //using namespace edm;
  
  //cout << "Run: " << event.id().run() << " Event: " << event.id().event() << endl;
  
  MuonPatternRecoDumper debug;
  
  // Get the RecTrack collection from the event
  Handle<reco::TrackCollection> staTracks;
  event.getByLabel(theSTAMuonLabel, staTracks);

  Handle<reco::TrackCollection> tkTracks;
  event.getByLabel(theTkTracksLabel, tkTracks);

  ESHandle<MagneticField> theMGField;
  eventSetup.get<IdealMagneticFieldRecord>().get(theMGField);

  //GlobalMuonTrackMatcher* theTrackMatcher = new GlobalMuonTrackMatcher(theTrackMatcherChi2Cut,&*theMGField,theUpdator);
  GlobalMuonTrackMatcher* theTrackMatcher = new GlobalMuonTrackMatcher(theTrackMatcherChi2Cut);

  //reco::TrackCollection::const_iterator staTrack;
 
  std::pair<bool, reco::TrackRef> bestMatch;
  std::pair<bool, double> bestChi2;
 
  cout<<"Reconstructed STAMuon tracks: " << staTracks->size() << endl;
  cout<<"Reconstructed Tracker tracks: " << tkTracks->size() << endl;
  for (unsigned int position = 0; position != staTracks->size(); ++position) {
    LogDebug("TrackMatchTest")<<"+++ New Track +++"<<endl;
    reco::TrackRef staTrack(staTracks,position);
    bestMatch = theTrackMatcher->matchOne(staTrack,tkTracks);
    if(bestMatch.first){
      bestChi2 = theTrackMatcher->match(*staTrack,*bestMatch.second);
      hChi2->Fill(bestChi2.second);
    }
  }
  
}

//define this as a plug-in
DEFINE_FWK_MODULE(GLBTrackMatchAnalyzer)
