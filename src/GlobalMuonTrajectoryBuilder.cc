/**
 *  Class: GlobalMuonTrajectoryBuilder
 *
 *  Description:
 *   Reconstruct muons starting
 *   from a muon track reconstructed
 *   in the standalone muon system (with DT, CSC and RPC
 *   information).
 *   It tries to reconstruct the corresponding
 *   track in the tracker and performs
 *   matching between the reconstructed tracks
 *   in the muon system and the tracker.
 *
 *
 *  $Date: 2006/11/03 19:59:29 $
 *  $Revision: 1.51 $
 *
 *  Authors :
 *  N. Neumeister            Purdue University
 *  C. Liu                   Purdue University
 *  A. Everett               Purdue University
 *  with contributions from: S. Lacaprara, J. Mumford, P. Traczyk
 *
 **/

#include "RecoMuon/GlobalTrackFinder/interface/GlobalMuonTrajectoryBuilder.h"

//---------------
// C++ Headers --
//---------------

#include <iostream>
#include <iomanip>
#include <algorithm>

//-------------------------------
// Collaborating Class Headers --
//-------------------------------

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/Event.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "CommonTools/Statistics/interface/ChiSquaredProbability.h"


#include "TrackingTools/TrackFitters/interface/RecHitLessByDet.h"
#include "TrackingTools/PatternTools/interface/TrajectoryMeasurement.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateOnSurface.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateTransform.h"

#include "DataFormats/DetId/interface/DetId.h"
#include "DataFormats/MuonDetId/interface/DTChamberId.h"
#include "DataFormats/MuonDetId/interface/CSCDetId.h"
#include "DataFormats/MuonDetId/interface/RPCDetId.h"

#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/TrackReco/interface/TrackExtraFwd.h"

#include "RecoTracker/CkfPattern/interface/CkfTrajectoryBuilder.h"
#include "RecoTracker/TkTrackingRegions/interface/RectangularEtaPhiTrackingRegion.h"

#include "RecoMuon/GlobalTrackFinder/interface/GlobalMuonTrackMatcher.h"
#include "RecoMuon/GlobalTrackFinder/interface/GlobalMuonSeedCleaner.h"

#include "RecoMuon/MeasurementDet/interface/MuonDetLayerMeasurements.h"
#include "RecoMuon/TransientTrackingRecHit/interface/MuonTransientTrackingRecHit.h"
#include "RecoMuon/TrackingTools/interface/MuonTrackReFitter.h"
#include "RecoMuon/TrackingTools/interface/MuonUpdatorAtVertex.h"
#include "RecoMuon/TrackingTools/interface/MuonCandidate.h"
#include "RecoMuon/TrackingTools/interface/MuonServiceProxy.h"

#include "RecoMuon/TrackingTools/interface/MuonTrackConverter.h"
//#include "RecoMuon/TrackerSeedGenerator/interface/TrackerSeedGenerator.h"

using namespace std;
using namespace edm;

//----------------
// Constructors --
//----------------

GlobalMuonTrajectoryBuilder::GlobalMuonTrajectoryBuilder(const edm::ParameterSet& par,
							 const MuonServiceProxy* service) : theService(service) {

  ParameterSet refitterPSet = par.getParameter<ParameterSet>("RefitterParameters");
  theRefitter = new MuonTrackReFitter(refitterPSet,theService);

  ParameterSet seedGeneratorPSet = par.getParameter<ParameterSet>("SeedGeneratorParameters");
  //theTkSeedGenerator = new TrackerSeedGenerator(seedGeneratorPSet,theService);  

  theLayerMeasurements = new MuonDetLayerMeasurements();
  
  tkSeedFlag = false;
  theTkTrackLabel = par.getParameter<string>("TkTrackCollectionLabel");  
  if (theTkTrackLabel == "") {
    LogInfo("GlobalMuonTrajectoryBuilder") << "TkTrackCollectionLabel unspecified" ;  
    tkSeedFlag = true;
  }

  theTrackConverter = new MuonTrackConverter(par,theService);
  theTrackMatcher = new GlobalMuonTrackMatcher(par,theService);

  theMuonHitsOption = par.getParameter<int>("MuonHitsOption");
  theDirection = static_cast<ReconstructionDirection>(par.getParameter<int>("Direction"));
  thePtCut = par.getParameter<double>("PtCut");
  theProbCut = par.getParameter<double>("Chi2ProbabilityCut");
  theHitThreshold = par.getParameter<int>("HitThreshold");
  theDTChi2Cut  = par.getParameter<double>("Chi2CutDT");
  theCSCChi2Cut = par.getParameter<double>("Chi2CutCSC");
  theRPCChi2Cut = par.getParameter<double>("Chi2CutRPC");

  theVertexPos = GlobalPoint(0.0,0.0,0.0);
  theVertexErr = GlobalError(0.0001,0.0,0.0001,0.0,0.0,28.09);

  convert = true;
  tkTrajsAvailable = false;
 
}


//--------------
// Destructor --
//--------------

GlobalMuonTrajectoryBuilder::~GlobalMuonTrajectoryBuilder() {

  if (theRefitter) delete theRefitter;
  if (theTrackMatcher) delete theTrackMatcher;
  if (theLayerMeasurements) delete theLayerMeasurements;
  if (theTrackConverter) delete theTrackConverter;

}


//
// set Event
//
void GlobalMuonTrajectoryBuilder::setEvent(const edm::Event& event) {
  
  // get tracker TrackCollection from Event
  event.getByLabel(theTkTrackLabel,allTrackerTracks);
  
  edm::Handle<std::vector<Trajectory> > handleTrackerTrajs;
  try
    {
      event.getByLabel(theTkTrackLabel,handleTrackerTrajs);
      tkTrajsAvailable = true;
      allTrackerTrajs = &*handleTrackerTrajs;   
      
      LogInfo("GlobalMuonTrajectoryBuilder") 
	<< "Tk Trajectories Found! " << endl;
    }
  catch (...)
    {
      LogInfo("GlobalMuonTrajectoryBuilder") 
	<< "No Tk Trajectories Found! " << endl;
      tkTrajsAvailable = false;
    }
  
  LogInfo("GlobalMuonTrajectoryBuilder") 
    << "Found " << allTrackerTracks->size() 
    << " tracker Tracks with label "<< theTkTrackLabel <<endl;
  
  theLayerMeasurements->setEvent(event);  

}


//
// reconstruct trajectories
//
MuonCandidate::CandidateContainer GlobalMuonTrajectoryBuilder::trajectories(const reco::TrackRef& staTrack) {
  TrackCand staCand = TrackCand(0,staTrack);
  return trajectories(staCand);
}

//
// reconstruct trajectories
//
MuonCandidate::CandidateContainer GlobalMuonTrajectoryBuilder::trajectories(const TrackCand& staCandIn) {

  // cut on muons with low momenta
  if ( (staCandIn).second->pt() < thePtCut || (staCandIn).second->innerMomentum().Rho() < thePtCut || (staCandIn).second->innerMomentum().R() < 2.5 ) return CandidateContainer();
  
  // convert the STA track into a Trajectory if Trajectory not already present
  TrackCand staCand;
  if(staCandIn.first == 0) {
    staCand = TrackCand(0,staCandIn.second);
    if ( convert ) {
      LogInfo("GlobalMuonTrajectoryBuilder") 
	<< "Making new StaTrackCand" << endl;
      TC staTrajs = theTrackConverter->convert(staCandIn.second);
      staCand = ( !staTrajs.empty() ) ? TrackCand(new Trajectory(staTrajs.front()),staCandIn.second) : TrackCand(0,staCandIn.second);
    }
  } else {
    LogInfo("GlobalMuonTrajectoryBuilder") 
      << "Using pre-made StaTrackCand" << endl;
    staCand = TrackCand(new Trajectory(*(staCandIn.first)),staCandIn.second);
  }

  // make tracker seeds and tracks  
  if( tkSeedFlag ) {
    //tkSeeds = theTkSeedGenerator->trajectorySeeds(staCand.first);
    //allTrackerTracks = makeTracks(tkSeeds); 
  }
  
  // convert tracks to TrackCands
  vector<TrackCand> tkTrackCands;
  for ( unsigned int position = 0; position != allTrackerTracks->size(); ++position ) {
    reco::TrackRef tkTrackRef(allTrackerTracks,position);
    TrackCand tkCand = TrackCand(0,tkTrackRef);
    if ( tkTrajsAvailable ) {
      std::vector<Trajectory>::const_iterator it = allTrackerTrajs->begin()+position;
      Trajectory trajRef = *it;
      if( trajRef.isValid() ) tkCand = TrackCand(&trajRef,tkTrackRef);
    } 
    tkTrackCands.push_back(tkCand);          
  }
  
  // select tracker tracks in eta-phi cone around muon
  vector<TrackCand> regionalTkTracks = chooseRegionalTrackerTracks(staCand,tkTrackCands);

  LogInfo("GlobalMuonTrajectoryBuilder") << "Found " << regionalTkTracks.size() << " tracks within region of interest" << endl;

  // match tracker tracks to muon track
  vector<TrackCand> trackerTracks = theTrackMatcher->match(staCand, regionalTkTracks);
  LogInfo("GlobalMuonTrajectoryBuilder") << "Found " << trackerTracks.size() << " matching tracker tracks within region of interest" << endl;

  // build a combined tracker-muon MuonCandidate
  CandidateContainer result = build(staCand, trackerTracks);

  // free memory
  if ( staCand.first != 0) {
    if ( staCand.first ) delete staCand.first;
  }
  for ( vector<TrackCand>::const_iterator is = regionalTkTracks.begin(); is != regionalTkTracks.end(); ++is) {
    if ( (*is).first ) delete (*is).first;
  }

  return result;

}


//
// select tracks within the region of interest
//
vector<GlobalMuonTrajectoryBuilder::TrackCand> 
GlobalMuonTrajectoryBuilder::chooseRegionalTrackerTracks(const TrackCand& staCand, 
                                                         const vector<TrackCand>& tkTs) const {
  
  // define eta-phi region
  RectangularEtaPhiTrackingRegion regionOfInterest = defineRegionOfInterest(staCand.second);
  
  vector<TrackCand> result;
  
  float deltaR = 1.0;
  
  int iPosition = 0;
  vector<TrackCand>::const_iterator is;
  for ( is = tkTs.begin(); is != tkTs.end(); ++is ) {
    iPosition++;
    double deltaEta = staCand.second->eta() - is->second->eta();
    double deltaPhi(fabs(Geom::Phi<float>(staCand.second->phi())-Geom::Phi<float>(is->second->phi())));
    double deltaR_tmp = sqrt(pow(deltaEta,2.) + pow(deltaPhi,2.));
    
    if(deltaR_tmp <= deltaR) {
      TrackCand tkCand = TrackCand(0,is->second);
      if(is->first == 0 || !(is->first->isValid()) ) {
	TC tkTrajs_tmp = theTrackConverter->convert(is->second);
	tkCand = ( !tkTrajs_tmp.empty() ) ? TrackCand(new Trajectory(tkTrajs_tmp.front()),is->second) : TrackCand(0,is->second);	
      } else {
	tkCand = TrackCand(new Trajectory(*(is->first)),is->second);
      }
      result.push_back(tkCand);      
    }
    
  }
  
  return result; 
}


//
// define a region of interest within the tracker
//
RectangularEtaPhiTrackingRegion GlobalMuonTrajectoryBuilder::defineRegionOfInterest(const reco::TrackRef& staTrack) const {

  // define tracker region of interest
  const math::XYZVector& mo = staTrack->innerMomentum();
  GlobalVector mom(mo.x(),mo.y(),mo.z()); 

  if ( staTrack->p() > 1.0 ) 
    mom = GlobalVector(staTrack->px(),staTrack->py(),staTrack->pz());
   
  float eta1   = mom.eta();
  float phi1   = mom.phi();
  float theta1 = mom.theta();

  float eta2 = 0.0;
  float phi2 = 0.0;
  float theta2 = 0.0;
  
  ConstMuonRecHitContainer recHits = theTrackConverter->getTransientMuonRecHits(*staTrack);
  ConstMuonRecHitPointer r = *(recHits.begin()+1);

  eta2   = r->globalPosition().eta();
  phi2   = r->globalPosition().phi();
  theta2 = r->globalPosition().theta();

  float deta(fabs(eta1-eta2));
  float dphi(fabs(Geom::Phi<float>(phi1)-Geom::Phi<float>(phi2)));

  double deltaEta = 0.1;//0.05
  double deltaPhi = 0.14;//0.07 // 5*ephi;
  double deltaZ   = min(15.9,3*sqrt(theVertexErr.czz()));
  double minPt    = max(1.5,mom.perp()*0.6);

  Geom::Phi<float> phi(phi1);
  Geom::Theta<float> theta(theta1);
  if ( deta > 0.06 ) {
    deltaEta += deta/2;
  } 
  if ( dphi > 0.07 ) {
    deltaPhi += 0.3; //0.15;
    if ( fabs(eta2) < 1.0 && mom.perp() < 6. ) deltaPhi = dphi;
  }
  if ( fabs(eta1) < 1.25 && fabs(eta1) > 0.8 ) deltaEta = max(0.07,deltaEta);
  if ( fabs(eta1) < 1.3  && fabs(eta1) > 1.0 ) deltaPhi = max(0.3,deltaPhi);

  deltaEta = max(0.3,deltaEta);
  deltaPhi = max(0.5,deltaPhi);

  GlobalVector direction(theta,phi,mom.perp());
  RectangularEtaPhiTrackingRegion rectRegion(direction, theVertexPos,
                                             minPt, 0.2, 
                                             deltaZ, deltaEta, deltaPhi);

  return rectRegion;

}


//
// build a combined tracker-muon trajectory
//
MuonCandidate::CandidateContainer GlobalMuonTrajectoryBuilder::build(const TrackCand& staCand, 
                                                                     const std::vector<TrackCand>& tkMatchedTracks) const {

  // MuonHitsOption: 0 - tracker only
  //                 1 - include all muon hits
  //                 2 - include only first muon hit(s)
  //                 3 - include only selected muon hits
  //                 4 - combined
  //

  if ( tkMatchedTracks.empty() ) return CandidateContainer();

  //
  // turn tkMatchedTracks into MuonCandidates
  //
  CandidateContainer tkTrajs;
  for (vector<TrackCand>::const_iterator tkt = tkMatchedTracks.begin(); tkt != tkMatchedTracks.end(); tkt++) {
    if ((*tkt).first != 0) {
      MuonCandidate* muonCand = new MuonCandidate(new Trajectory(*(*tkt).first),staCand.second,(*tkt).second);
      tkTrajs.push_back(muonCand);
    }
  }

  //
  // check and select muon measurements and 
  // measure occupancy in muon stations
  //   
  vector<int> stationHits(4,0);
  ConstRecHitContainer muonRecHits1; // all muon rechits
  ConstRecHitContainer muonRecHits2; // only first muon rechits
  if ( theMuonHitsOption > 0 ) checkMuonHits(*(staCand.second), muonRecHits1, muonRecHits2, stationHits);

  //
  // add muon hits and refit/smooth trajectories
  //
  CandidateContainer refittedResult;

  if ( theMuonHitsOption > 0 ) {

    for ( CandidateContainer::const_iterator it = tkTrajs.begin(); it != tkTrajs.end(); it++ ) {

      // cut on tracks with low momenta
      const GlobalVector& mom = (*it)->trajectory()->lastMeasurement().updatedState().globalMomentum();
      if ( mom.mag() < 2.5 || mom.perp() < thePtCut ) continue;
      ConstRecHitContainer trackerRecHits = (*it)->trajectory()->recHits();

      if ( theDirection == insideOut ) {
	reverse(trackerRecHits.begin(),trackerRecHits.end());
      }

      TrajectoryMeasurement firstTM = ( theDirection == outsideIn ) ? (*it)->trajectory()->firstMeasurement() : (*it)->trajectory()->lastMeasurement();
      TrajectoryStateOnSurface firstTsos = firstTM.updatedState();
      firstTsos.rescaleError(100.);

      TC refitted1,refitted2,refitted3;
      vector<Trajectory*> refit(4);
      MuonCandidate* finalTrajectory = 0;
      // tracker only track
      refit[0] = (*it)->trajectory();

      ConstRecHitContainer rechits(trackerRecHits);
      
      // full track with all muon hits
      if ( theMuonHitsOption == 1 || theMuonHitsOption == 3 || theMuonHitsOption == 4 ) {
	rechits.insert(rechits.end(), muonRecHits1.begin(), muonRecHits1.end());
	LogDebug("GlobalMuonTrajectoryBuilder") << "Number of hits: " << rechits.size();
	refitted1 = theRefitter->trajectories((*it)->trajectory()->seed(),rechits,firstTsos);

	if ( refitted1.size() == 1 ) {
	  refit[1] = &(*refitted1.begin());
	  if ( theMuonHitsOption == 1 ) {
            finalTrajectory = new MuonCandidate(new Trajectory(*refitted1.begin()), (*it)->muonTrack(), (*it)->trackerTrack());
             if ( (*it)->trajectory() ) delete (*it)->trajectory();
             if ( *it ) delete (*it);
          }
	}
      }

      // only first muon hits
      if ( theMuonHitsOption == 2 || theMuonHitsOption == 4 ) {
	rechits = trackerRecHits;
  	rechits.insert(rechits.end(), muonRecHits2.begin(), muonRecHits2.end());

	LogDebug("GlobalMuonTrajectoryBuilder")<< "Number of hits: "<<rechits.size();
	
	refitted2 = theRefitter->trajectories((*it)->trajectory()->seed(),rechits,firstTsos);
	if ( refitted2.size() == 1 ) {
	  refit[2] = &(*refitted2.begin());
	  if ( theMuonHitsOption == 2 ) {
            finalTrajectory = new MuonCandidate(new Trajectory(*refitted2.begin()), (*it)->muonTrack(), (*it)->trackerTrack());
            if ( (*it)->trajectory() ) delete (*it)->trajectory();
            if ( *it ) delete (*it);
          }
	}
      } 

      // only selected muon hits
      if ( theMuonHitsOption == 3 || theMuonHitsOption == 4 ) {
	ConstRecHitContainer muonRecHits3;
	if ( refitted1.size() == 1 ) muonRecHits3 = selectMuonHits(*refitted1.begin(),stationHits);
	rechits = trackerRecHits;
	rechits.insert(rechits.end(), muonRecHits3.begin(), muonRecHits3.end());
	
	LogDebug("GlobalMuonTrajectoryBuilder") << "Number of hits: " << rechits.size();
	
	refitted3 = theRefitter->trajectories((*it)->trajectory()->seed(),rechits,firstTsos);
	if ( refitted3.size() == 1 ) {
	  refit[3] = &(*refitted3.begin());
	  if ( theMuonHitsOption == 3 ) {
            finalTrajectory = new MuonCandidate(new Trajectory(*refitted3.begin()), (*it)->muonTrack(), (*it)->trackerTrack());
            if ( (*it)->trajectory() ) delete (*it)->trajectory();
            if ( *it ) delete (*it);
          }
	}
      }

      if ( theMuonHitsOption == 4 ) {
	finalTrajectory = new MuonCandidate(new Trajectory(*chooseTrajectory(refit)), (*it)->muonTrack(), (*it)->trackerTrack());
        if ( (*it)->trajectory() ) delete (*it)->trajectory();
        if ( *it ) delete (*it);
      } 
      
      if ( finalTrajectory ) {
	refittedResult.push_back(finalTrajectory);
      }
    }

  }
  else {
    refittedResult = tkTrajs;
  }
  //    int nRefitted = refittedResult.size();
  
  //FIXME: IMPLEMENT ME  
  //
  // muon trajectory cleaner
  //
  //    TrajectoryCleaner* mcleaner = new L3MuonTrajectoryCleaner();
  //    mcleaner->clean(refittedResult);
  //   delete mcleaner;
  
  //    if ( cout.testOut ) {
  //    cout.testOut << "seeds    : " << setw(3) << nSeeds << endl; 
  //      cout.testOut << "raw      : " << setw(3) << nRaw << endl;
  //    cout.testOut << "smoothed : " << setw(3) << nSmoothed << endl;
  //      cout.testOut << "cleaned  : " << setw(3) << nCleaned << endl;
  //      cout.testOut << "refitted : " << setw(3) << nRefitted << endl;
  //  }
  
  //
  //Perform a ghost suppression on all candidates, not only on those coming
  //from the same seed (RecMuon)
  //FIXME: IMPLEMENT ME

  return refittedResult;

}


//
//
//
void GlobalMuonTrajectoryBuilder::checkMuonHits(const reco::Track& muon, 
						ConstRecHitContainer& all,
						ConstRecHitContainer& first,
						std::vector<int>& hits) const {

  int dethits[4];
  for ( int i=0; i<4; i++ ) hits[i]=dethits[i]=0;
  
  ConstMuonRecHitContainer muonRecHits = theTrackConverter->getTransientMuonRecHits(muon);
  
  // loop through all muon hits and calculate the maximum # of hits in each chamber      
  for (ConstMuonRecHitContainer::const_iterator imrh = muonRecHits.begin(); imrh != muonRecHits.end(); imrh++ ) { 
    if ( !(*imrh)->isValid() ) continue;
    
    if ( theMuonHitsOption == 3 || theMuonHitsOption == 4 ) {
      
      int station = 0;
      int detRecHits = 0;
      
      DetId id = (*imrh)->geographicalId();
      
      const DetLayer* layer = theService->detLayerGeometry()->idToLayer(id);
      MuonRecHitContainer dRecHits = theLayerMeasurements->recHits(layer);
      
      // get station of hit if it is in DT
      if ( (**imrh).isDT() ) {
        DTChamberId did(id.rawId());
        station = did.station();
        float coneSize = 10.0;
	
        bool hitUnique = false;
        ConstRecHitContainer all2dRecHits;
        for (MuonRecHitContainer::const_iterator ir = dRecHits.begin(); ir != dRecHits.end(); ir++ ) {
          if ( (**ir).dimension() == 2 ) {
            hitUnique = true;
            if ( all2dRecHits.size() > 0 ) {
              for (ConstRecHitContainer::const_iterator iir = all2dRecHits.begin(); iir != all2dRecHits.end(); iir++ ) 
		if (((*iir)->localPosition()-(*ir)->localPosition()).mag()<0.01) hitUnique = false;
            } //end of if ( all2dRecHits.size() > 0 )
            if ( hitUnique ) all2dRecHits.push_back((*ir).get()); //FIXME!!
          } else {
            ConstRecHitContainer sRecHits = (**ir).transientHits();
            for (ConstRecHitContainer::const_iterator iir = sRecHits.begin(); iir != sRecHits.end(); iir++ ) {
              if ( (*iir)->dimension() == 2 ) {
                hitUnique = true;
                if ( all2dRecHits.size() > 0 ) {
                  for (ConstRecHitContainer::const_iterator iiir = all2dRecHits.begin(); iiir != all2dRecHits.end(); iiir++ ) 
		    if (((*iiir)->localPosition()-(*iir)->localPosition()).mag()<0.01) hitUnique = false;
                }//end of if ( all2dRecHits.size() > 0 )
              }//end of if ( (*iir).dimension() == 2 ) 
              if ( hitUnique )
		all2dRecHits.push_back(*iir);
              break;
            }//end of for sRecHits
          }// end of else
	}// end of for loop over dRecHits
	for (ConstRecHitContainer::const_iterator ir = all2dRecHits.begin(); ir != all2dRecHits.end(); ir++ ) {
	  double rhitDistance = ((*ir)->localPosition()-(**imrh).localPosition()).mag();
	  if ( rhitDistance < coneSize ) detRecHits++;
	  LogDebug("GlobalMuonTrajectoryBuilder") << " Station "<<station<<" DT "<<(*ir)->dimension()<<" " << (*ir)->localPosition()
						     <<" Distance: "<< rhitDistance<<" recHits: "<< detRecHits;
	}// end of for all2dRecHits
      }// end of if DT
      // get station of hit if it is in CSC
      else if ( (**imrh).isCSC() ) {
	CSCDetId did(id.rawId());
	station = did.station();
	
	float coneSize = 10.0;
	
	for (MuonRecHitContainer::const_iterator ir = dRecHits.begin(); ir != dRecHits.end(); ir++ ) {
	  double rhitDistance = ((**ir).localPosition()-(**imrh).localPosition()).mag();
	  if ( rhitDistance < coneSize ) detRecHits++;
	  LogDebug("GlobalMuonTrajectoryBuilder") << " Station " << station << " CSC "<<(**ir).dimension()<<" "<<(**ir).localPosition()
                                                  << " Distance: "<< rhitDistance<<" recHits: "<<detRecHits;
	}
      }
      // get station of hit if it is in RPC
      else if ( (**imrh).isRPC() ) {
	RPCDetId rpcid(id.rawId());
	station = rpcid.station();
	float coneSize = 100.0;
	for (MuonRecHitContainer::const_iterator ir = dRecHits.begin(); ir != dRecHits.end(); ir++ ) {
	  double rhitDistance = ((**ir).localPosition()-(**imrh).localPosition()).mag();
	  if ( rhitDistance < coneSize ) detRecHits++;
	  LogDebug("GlobalMuonTrajectoryBuilder")<<" Station "<<station<<" RPC "<<(**ir).dimension()<<" "<< (**ir).localPosition()
						     <<" Distance: "<<rhitDistance<<" recHits: "<<detRecHits;
	}
      }
      else {
	continue;      
      }
      
      if ( (station > 0) && (station < 5) ) {
	int detHits = dRecHits.size();
	dethits[station-1] += detHits;
	if ( detRecHits > hits[station-1] ) hits[station-1] = detRecHits;
      }
    } //end of if option 3, 4
    all.push_back((*imrh).get());
  } // end of loop over muon rechits
  if ( theMuonHitsOption == 3 || theMuonHitsOption == 4 )  {
    for ( int i = 0; i < 4; i++ ) {
      LogDebug("GlobalMuonTrajectoryBuilder")<<"Station "<<i+1<<": "<<hits[i]<<" "<<dethits[i]; 
    }
  }     
  
  //
  // check order of muon measurements
  //
  if ((*all.begin())->globalPosition().mag() >
      (*(all.end()-1))->globalPosition().mag() ) {
    LogDebug("GlobalMuonTrajectoryBuilder")<< "reverse order: ";
    sort(all.begin(),all.end(),RecHitLessByDet(alongMomentum));
  }
  
  
  int station1 = -999;
  int station2 = -999;
  
  for (ConstRecHitContainer::const_iterator ihit = all.begin(); ihit != all.end(); ihit++ ) {
    if ( !(*ihit)->isValid() ) continue;
    station1 = -999; station2 = -999;
    
    // store muon hits one at a time.
    first.push_back(*ihit);
    
    ConstMuonRecHitPointer immrh = dynamic_cast<const MuonTransientTrackingRecHit*>((*ihit).get()); //FIXME
    DetId id = immrh->geographicalId();
    
    // get station of 1st hit if it is in DT
    if ( (*immrh).isDT()  ) {
      DTChamberId did(id.rawId());
      station1 = did.station();
    }
    // otherwise get station of 1st hit if it is in CSC
    else if  ( (*immrh).isCSC() ) {
      CSCDetId did(id.rawId());
      station1 = did.station();
    }
    
    // check next RecHit
    ConstRecHitContainer::const_iterator nexthit(ihit);
    nexthit++;
    
    if ( ( nexthit != all.end()) && (*nexthit)->isValid() ) {
      
      ConstMuonRecHitPointer immrh2 = dynamic_cast<const MuonTransientTrackingRecHit*>((*nexthit).get());
      DetId id2 = immrh2->geographicalId();
      
      // get station of 1st hit if it is in DT
      if ( (*immrh2).isDT()  ) {
        DTChamberId did2(id2.rawId());
        station2 = did2.station();
      }
      // otherwise get station of 1st hit if it is in CSC
      else if  ( (*immrh2).isCSC() ) {
        CSCDetId did2(id2.rawId());
        station2 = did2.station();
      }
      
      // 1st hit is in station 1 and second hit is in a different station
      // or an rpc (if station = -999 it could be an rpc hit)
      if ( (station1 != -999) && ((station2 == -999) || (station2 > station1)) ) {
	LogDebug("GlobalMuonTrajectoryBuilder") << "checkMuonHits:";
	LogDebug("GlobalMuonTrajectoryBuilder") << " station 1 = "<<station1 
						   <<", r = "<< (*ihit)->globalPosition().perp()
						   <<", z = "<< (*ihit)->globalPosition().z() << ", "; 
	
	LogDebug("GlobalMuonTrajectoryBuilder") << " station 2 = " << station2
						   <<", r = "<<(*(nexthit))->globalPosition().perp()
						   <<", z = "<<(*(nexthit))->globalPosition().z() << ", ";
	return;
      }
    }
    else if ( (nexthit == all.end()) && (station1 != -999) ) {
      LogDebug("GlobalMuonTrajectoryBuilder") << "checkMuonHits:";
      LogDebug("GlobalMuonTrajectoryBuilder") << " station 1 = "<< station1
                                              << ", r = " << (*ihit)->globalPosition().perp()
                                              << ", z = " << (*ihit)->globalPosition().z() << ", "; 
      return;
    }
  }
  // if none of the above is satisfied, return blank vector.
  first.clear();
  
}


//
// select muon hits compatible with trajectory; 
// check hits in chambers with showers
//
GlobalMuonTrajectoryBuilder::ConstRecHitContainer 
GlobalMuonTrajectoryBuilder::selectMuonHits(const Trajectory& traj, 
                                            const std::vector<int>& hits) const {

  ConstRecHitContainer muonRecHits;
  const double globalChi2Cut = 200.0;

  vector<TrajectoryMeasurement> muonMeasurements = traj.measurements(); 

  // loop through all muon hits and skip hits with bad chi2 in chambers with high occupancy      
  for (std::vector<TrajectoryMeasurement>::const_iterator im = muonMeasurements.begin(); im != muonMeasurements.end(); im++ ) {

    if ( !(*im).recHit()->isValid() ) continue;
    if ( (*im).recHit()->det()->geographicalId().det() != DetId::Muon ) continue;
    ConstMuonRecHitPointer immrh = dynamic_cast<const MuonTransientTrackingRecHit*>((*im).recHit().get());

    DetId id = immrh->geographicalId();
    int station = 0;
    int threshold = 0;
    double chi2Cut = 0.0;

    // get station of hit if it is in DT
    if ( (*immrh).isDT() ) {
      DTChamberId did(id.rawId());
      station = did.station();
      threshold = theHitThreshold;
      chi2Cut = theDTChi2Cut;
    }
    // get station of hit if it is in CSC
    else if ( (*immrh).isCSC() ) {
      CSCDetId did(id.rawId());
      station = did.station();
      threshold = theHitThreshold;
      chi2Cut = theCSCChi2Cut;
    }
    // get station of hit if it is in RPC
    else if ( (*immrh).isRPC() ) {
      RPCDetId rpcid(id.rawId());
      station = rpcid.station();
      threshold = theHitThreshold;
      chi2Cut = theRPCChi2Cut;
    }
    else {
      continue;
    }

    double chi2ndf = (*im).estimate()/(*im).recHit()->dimension();  

    bool keep = true;
    if ( (station > 0) && (station < 5) ) {
      if ( hits[station-1] > threshold ) keep = false;
    }   
    
    if ( (keep || ( chi2ndf < chi2Cut )) && ( chi2ndf < globalChi2Cut ) ) {
      muonRecHits.push_back((*im).recHit());
    } else {
      LogDebug("GlobalMuonTrajectoryBuilder")
	<< "Skip hit: " << id.det() << " " << station << ", " 
	<< chi2ndf << " (" << chi2Cut << " chi2 threshold) " 
	<< hits[station-1] << endl;
    }

  }
  
  //
  // check order of rechits
  //
  reverse(muonRecHits.begin(),muonRecHits.end());
  return muonRecHits;

}


//
// choose final trajectory
//
const Trajectory* GlobalMuonTrajectoryBuilder::chooseTrajectory(const std::vector<Trajectory*>& t) const {

  Trajectory* result = 0;
 
  double prob0 = ( t[0] ) ? trackProbability(*t[0]) : 0.0;
  double prob1 = ( t[1] ) ? trackProbability(*t[1]) : 0.0;
  double prob2 = ( t[2] ) ? trackProbability(*t[2]) : 0.0;
  double prob3 = ( t[3] ) ? trackProbability(*t[3]) : 0.0; 

  LogDebug("GlobalMuonTrajectoryBuilder") << "Probabilities: " << prob0 << " " << prob1 << " " << prob2 << " " << prob3 << endl;

  if ( t[1] ) result = t[1];
  if ( (t[1] == 0) && t[3] ) result = t[3];
  
  if ( t[1] && t[3] && ( (prob1 - prob3) > 0.05 )  )  result = t[3];

  if ( t[0] && t[2] && fabs(prob2 - prob0) > theProbCut ) {
    LogDebug("GlobalMuonTrajectoryBuilder") << "select Tracker only: -log(prob) = " << prob0 << endl;
    result = t[0];
    return result;
  }

  if ( (t[1] == 0) && (t[3] == 0) && t[2] ) result = t[2];

  Trajectory* tmin = 0;
  double probmin = 0.0;
  if ( t[1] && t[3] ) {
    probmin = prob3; tmin = t[3];
    if ( prob1 < prob3 ) { probmin = prob1; tmin = t[1]; }
  }
  else if ( (t[3] == 0) && t[1] ) { 
    probmin = prob1; tmin = t[1]; 
  }
  else if ( (t[1] == 0) && t[3] ) {
    probmin = prob3; tmin = t[3]; 
  }

  if ( tmin && t[2] && ( (probmin - prob2) > 3.5 )  ) {
    result = t[2];
  }

  return result;

}


//
// calculate the tail probability (-ln(P)) of a fit
//
double GlobalMuonTrajectoryBuilder::trackProbability(const Trajectory& track) const {

  int nDOF = 0;
  ConstRecHitContainer rechits = track.recHits();
  for ( ConstRecHitContainer::const_iterator i = rechits.begin(); i != rechits.end(); ++i ) {
    if ((*i)->isValid()) nDOF += (*i)->dimension();
  }
  
  nDOF = max(nDOF - 5, 0);
  double prob = -LnChiSquaredProbability(track.chiSquared(), nDOF);
  
  return prob;

}


//
// print RecHits
//
void GlobalMuonTrajectoryBuilder::printHits(const ConstRecHitContainer& hits) const {

  LogInfo("GlobalMuonTrajectoryBuilder") << "Used RecHits: ";
  for (ConstRecHitContainer::const_iterator ir = hits.begin(); ir != hits.end(); ir++ ) {
    if ( !(*ir)->isValid() ) {
      LogInfo("GlobalMuonTrajectoryBuilder") << "invalid RecHit";
      continue; 
    }

    const GlobalPoint& pos = (*ir)->globalPosition();
    LogInfo("GlobalMuonTrajectoryBuilder") 
    << "r = " << sqrt(pos.x() * pos.x() + pos.y() * pos.y())
    << "  z = " << pos.z()
    << "  dimension = " << (*ir)->dimension()
    << "  " << (*ir)->det()->geographicalId().det()
    << "  " << (*ir)->det()->subDetector();
  }

}
