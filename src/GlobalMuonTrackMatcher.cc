/** \class GlobalMuonTrackMatcher
 *  match standalone muon track with tracker tracks
 *
 *  $Date: 2006/08/09 16:40:29 $
 *  $Revision: 1.18 $
 *  \author Chang Liu  - Purdue University
 *  \author Norbert Neumeister - Purdue University
 */

#include "RecoMuon/GlobalTrackFinder/interface/GlobalMuonTrackMatcher.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateOnSurface.h"
#include "TrackingTools/TransientTrack/interface/TransientTrack.h"
#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h" 
#include "RecoMuon/TrackingTools/interface/MuonUpdatorAtVertex.h"
#include "Geometry/TrackerGeometryBuilder/interface/TrackerGeometry.h"
#include "Geometry/Records/interface/GlobalTrackingGeometryRecord.h"
#include "Geometry/CommonDetUnit/interface/GlobalTrackingGeometry.h"

using namespace std;
using namespace edm;
//
// constructor
//
GlobalMuonTrackMatcher::GlobalMuonTrackMatcher(double chi2, 
                                               const edm::ESHandle<MagneticField> field, 
                                               MuonUpdatorAtVertex* updator) {

  theMaxChi2 = chi2;
  theMinP = 2.5;
  theMinPt = 1.0;
  theField = field;
  theUpdator = updator;

}


//
//
//
GlobalMuonTrackMatcher::GlobalMuonTrackMatcher(double chi2) {

  theMaxChi2 = chi2;
  theMinP = 2.5;
  theMinPt = 1.0;
  theUpdator = new MuonUpdatorAtVertex();

}


//
//
//
void GlobalMuonTrackMatcher::setES(const edm::EventSetup& setup) {

  setup.get<IdealMagneticFieldRecord>().get(theField);
  setup.get<GlobalTrackingGeometryRecord>().get(theTrackingGeometry); 
  theUpdator->setES(setup);

}


//
// choose the tracker Track from a TrackCollection which has smallest chi2 with
// a given standalone Track
//
std::pair<bool, reco::TrackRef> 
GlobalMuonTrackMatcher::matchOne(const reco::TrackRef& staT, 
                                 const edm::Handle<reco::TrackCollection>& tkTs) const {

  bool hasMatchTk = false;
  reco::TrackRef result;
  double minChi2 = theMaxChi2;
  
  reco::TransientTrack staTT(staT,&*theField,theTrackingGeometry);
  TrajectoryStateOnSurface innerMuTsos = staTT.innermostMeasurementState();

  // extrapolate innermost standalone TSOS to outer tracker surface
  TrajectoryStateOnSurface tkTsosFromMu = 
    theUpdator->stateAtTracker(innerMuTsos);

  for (unsigned int position = 0; position < tkTs->size(); ++position) {
    reco::TrackRef tkTRef(tkTs,position);
    reco::TransientTrack tkTT(tkTRef,&*theField,theTrackingGeometry);
    // make sure the tracker Track has enough momentum to reach muon chambers
    const GlobalVector& mom = tkTT.impactPointState().globalMomentum();
    if ( mom.mag() < theMinP || mom.perp() < theMinPt ) continue;

    TrajectoryStateOnSurface outerTkTsos = tkTT.outermostMeasurementState();
    
    // extrapolate outermost tracker measurement TSOS to outer tracker surface
    TrajectoryStateOnSurface tkTsosFromTk = 
      theUpdator->stateAtTracker(outerTkTsos);

    std::pair<bool,double> check = match(tkTsosFromMu,tkTsosFromTk);

    if (!check.first) continue;

    if (check.second < minChi2) { 
      hasMatchTk = true;
      minChi2 = check.second;
      result = tkTRef;
    } 
  }     

  return(std::pair<bool, reco::TrackRef>(hasMatchTk, result));

}


//
// choose a vector of tracker Tracks from a TrackCollection that has Chi2 less than
// theMaxChi2, for a given standalone Track
//
std::vector<reco::TrackRef>
GlobalMuonTrackMatcher::match(const reco::TrackRef& staT, 
                              const edm::Handle<reco::TrackCollection>& tkTs) const {

  std::vector<reco::TrackRef> result;

  reco::TransientTrack staTT(staT,&*theField,theTrackingGeometry);

  TrajectoryStateOnSurface innerMuTsos = staTT.innermostMeasurementState();

  // extrapolate innermost standalone TSOS to outer tracker surface
  TrajectoryStateOnSurface tkTsosFromMu = 
    theUpdator->stateAtTracker(innerMuTsos);
 
  for (unsigned int position = 0; position < tkTs->size(); position++) {
    reco::TrackRef tkTRef(tkTs,position);
    reco::TransientTrack tkTT(tkTRef,&*theField,theTrackingGeometry);
    // make sure the tracker Track has enough momentum to reach muon chambers
    const GlobalVector& mom = tkTT.impactPointState().globalMomentum();
    if ( mom.mag() < theMinP || mom.perp() < theMinPt ) continue;
    
    TrajectoryStateOnSurface outerTkTsos = tkTT.outermostMeasurementState();
    
    // extrapolate outermost tracker measurement TSOS to outer tracker surface
    TrajectoryStateOnSurface tkTsosFromTk = theUpdator->stateAtTracker(outerTkTsos);
    
    std::pair<bool,double> check = match(tkTsosFromMu,tkTsosFromTk);
    if (check.first) result.push_back(tkTRef);
  }
  
  return result;
  
}


//
//
//
std::vector<reco::TrackRef>
GlobalMuonTrackMatcher::match(const reco::TrackRef& staT, 
                              const std::vector<reco::TrackRef>& tkTs) const {

  std::vector<reco::TrackRef> result;

  reco::TransientTrack staTT(staT,&*theField,theTrackingGeometry);

  TrajectoryStateOnSurface innerMuTsos = staTT.innermostMeasurementState();

  // extrapolate innermost standalone TSOS to outer tracker surface
  TrajectoryStateOnSurface tkTsosFromMu = 
    theUpdator->stateAtTracker(innerMuTsos);

  for (std::vector<reco::TrackRef>::const_iterator tkTRef = tkTs.begin();
       tkTRef != tkTs.end(); tkTRef++) {
    reco::TransientTrack tkTT(*tkTRef, &*theField,theTrackingGeometry);

    // make sure the tracker Track has enough momentum to reach muon chambers
    const GlobalVector& mom = tkTT.impactPointState().globalMomentum();
    if ( mom.mag() < theMinP || mom.perp() < theMinPt ) continue;
    
    TrajectoryStateOnSurface outerTkTsos = tkTT.outermostMeasurementState();
    
    // extrapolate outermost tracker measurement TSOS to outer tracker surface
    TrajectoryStateOnSurface tkTsosFromTk = 
      theUpdator->stateAtTracker(outerTkTsos);

    std::pair<bool,double> check = match(tkTsosFromMu,tkTsosFromTk);

    bool goodCoords = false;
    float dx(fabs(tkTsosFromMu.globalPosition().x() - tkTsosFromTk.globalPosition().x()));
    float dy(fabs(tkTsosFromMu.globalPosition().y() - tkTsosFromTk.globalPosition().y()));
    float dz(fabs(tkTsosFromMu.globalPosition().z() - tkTsosFromTk.globalPosition().z()));

    float dd = 5.0;

    if( (dx < dd) && (dy < dd) ) goodCoords = true;

    if ( check.first || goodCoords ) result.push_back(*tkTRef);
  }

  return result;

}

//
// determine if two TrackRefs are compatible
// by comparing their TSOSs on the outer Tracker surface
//
std::pair<bool,double> 
GlobalMuonTrackMatcher::match(const reco::TrackRef& sta, 
                              const reco::TrackRef& tk) const {
  reco::TransientTrack staT(sta,&*theField,theTrackingGeometry);  
  reco::TransientTrack tkT(tk,&*theField,theTrackingGeometry);
  
  // make sure the tracker Track has enough momentum to reach muon chambers
  const GlobalVector& mom = tkT.impactPointState().globalMomentum();
  if ( mom.mag() < theMinP || mom.perp() < theMinPt )
    return std::pair<bool,double>(false,0);
  
  TrajectoryStateOnSurface outerTkTsos = tkT.outermostMeasurementState();
  
  TrajectoryStateOnSurface innerMuTsos = staT.innermostMeasurementState();
  
  // extrapolate innermost standalone TSOS to outer tracker surface
  TrajectoryStateOnSurface tkTsosFromMu = theUpdator->stateAtTracker(innerMuTsos);
  
  // extrapolate outermost tracker measurement TSOS to outer tracker surface
  TrajectoryStateOnSurface tkTsosFromTk = theUpdator->stateAtTracker(outerTkTsos);
  
  // compare the TSOSs on outer tracker surface
  return match(tkTsosFromMu,tkTsosFromTk);
  
}

//
// determine if two Tracks are compatible
// by comparing their TSOSs on the outer Tracker surface
//
std::pair<bool,double> 
GlobalMuonTrackMatcher::match(const reco::Track& sta, 
                              const reco::Track& tk) const {
  reco::TransientTrack staT(sta,&*theField,theTrackingGeometry);  
  reco::TransientTrack tkT(tk,&*theField,theTrackingGeometry);
  //FIXME: caution! no Track * stored in TransientTrack.

  // make sure the tracker Track has enough momentum to reach muon chambers
  const GlobalVector& mom = tkT.impactPointState().globalMomentum();
  if ( mom.mag() < theMinP || mom.perp() < theMinPt )
    return std::pair<bool,double>(false,0);
  
  TrajectoryStateOnSurface outerTkTsos = tkT.outermostMeasurementState();
  
  TrajectoryStateOnSurface innerMuTsos = staT.innermostMeasurementState();
  
  // extrapolate innermost standalone TSOS to outer tracker surface
  TrajectoryStateOnSurface tkTsosFromMu = theUpdator->stateAtTracker(innerMuTsos);
  
  // extrapolate outermost tracker measurement TSOS to outer tracker surface
  TrajectoryStateOnSurface tkTsosFromTk = theUpdator->stateAtTracker(outerTkTsos);
  
  // compare the TSOSs on outer tracker surface
  return match(tkTsosFromMu,tkTsosFromTk);
  
}


//
// determine if two TSOSs are compatible, they should be on same surface
// 
std::pair<bool,double> 
GlobalMuonTrackMatcher::match(const TrajectoryStateOnSurface& tsos1, 
                              const TrajectoryStateOnSurface& tsos2) const {

  AlgebraicVector v(tsos1.localParameters().vector() - tsos2.localParameters().vector());
  AlgebraicSymMatrix m(tsos1.localError().matrix() + tsos2.localError().matrix());
  int ierr;
  m.invert(ierr);
  // if (ierr != 0) throw exception;
  double est = m.similarity(v);

  return ( est > theMaxChi2 ) ? std::pair<bool,double>(false,est) : std::pair<bool,double>(true,est);

}

