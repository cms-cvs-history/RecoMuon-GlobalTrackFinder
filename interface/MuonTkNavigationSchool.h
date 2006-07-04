#ifndef Navigation_MuonTkNavigationSchool_H
#define Navigation_MuonTkNavigationSchool_H

/** \class MuonTkNavigationSchool
 *
 *  Navigation School for both Muon and Tk
 *  different from the one in ORCA
 *
 *  $Date:  $
 *  $Revision:  $
 *
 * \author : Chang Liu - Purdue University
 * \author Stefano Lacaprara - INFN Padova 
 *         Gilles De Lentdecker - IIHE Brussels 
 *
 *  
 */


#include "TrackingTools/DetLayers/interface/NavigationSchool.h"
#include "RecoTracker/TkDetLayers/interface/GeometricSearchTracker.h"
#include "RecoMuon/DetLayers/interface/MuonDetLayerGeometry.h"
#include "RecoMuon/Navigation/interface/MuonLayerSort.h"

class DetLayer;
class BarrelDetLayer;
class ForwardDetLayer;
class SymmetricLayerFinder;
class NavigableLayer;
class SimpleBarrelNavigableLayer;
class SimpleForwardNavigableLayer;
class MuonBarrelNavigableLayer;
class MuonForwardNavigableLayer;
class MagneticField;

#include <vector>

class MuonTkNavigationSchool : public NavigationSchool {

  public:

    /// Constructor
    MuonTkNavigationSchool(const MuonDetLayerGeometry*, const GeometricSearchTracker*, const MagneticField*);
    /// Destructor
    ~MuonTkNavigationSchool();
    /// return a vector of NavigableLayer*, from base class
    virtual std::vector<NavigableLayer*> navigableLayers() const;

  private:
    /// add barrel layer
    void addBarrelLayer(BarrelDetLayer*);
    /// add endcap layer (backward and forward)
    void addEndcapLayer(ForwardDetLayer*);
    /// link barrel layers
    void linkBarrelLayers();
    /// link endcap layers
    void linkEndcapLayers(const MapE&, 
                          std::vector<MuonForwardNavigableLayer*>&,
                          std::vector<SimpleForwardNavigableLayer*>&);

    float barrelLength();

    float calculateEta(float r, float z) const;

  private:
    typedef std::vector<const DetLayer*>         DLC;
    typedef std::vector<BarrelDetLayer*>         BDLC;
    typedef std::vector<ForwardDetLayer*>        FDLC;
    typedef DLC::iterator                        DLI;
    typedef BDLC::iterator                       BDLI;
    typedef FDLC::iterator                       FDLI;
    typedef BDLC::const_iterator                 ConstBDLI;
    typedef FDLC::const_iterator                 ConstFDLI;

    MapB theBarrelLayers;
    MapE theForwardLayers;   // +z
    MapE theBackwardLayers;  // -z

    std::vector<SimpleBarrelNavigableLayer*>  theTkBarrelNLC;
    std::vector<SimpleForwardNavigableLayer*> theTkForwardNLC;
    std::vector<SimpleForwardNavigableLayer*> theTkBackwardNLC;

    std::vector<MuonBarrelNavigableLayer*> theMuonBarrelNLC;
    std::vector<MuonForwardNavigableLayer*> theMuonForwardNLC;
    std::vector<MuonForwardNavigableLayer*> theMuonBackwardNLC;

    const MuonDetLayerGeometry* theMuonDetLayerGeometry;
    const GeometricSearchTracker* theGeometricSearchTracker;
    const MagneticField* theMagneticField;
    float theBarrelLength;

};

#endif