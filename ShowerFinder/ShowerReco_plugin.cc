////////////////////////////////////////////////////////////////////////
/// \file  ShowerFinder.cxx
/// \brief Reconstruct Showers 
///
/// \version $Id: ShowerFinder.cxx,v 0.1 11/04/2010 12:45:16 PM  brossi strauss $
/// \author brossi@lhep.unibe.ch
/// \author strauss@lhep.unibe.ch
////////////////////////////////////////////////////////////////////////
// This class solves the following problem:
//
// Automaitc shower reconstruction and distinguish between electron and gamma

// Framework includes
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ServiceRegistry/interface/ServiceMaker.h"

#include "ShowerFinder/ShowerReco.h"


namespace shwf {

  DEFINE_FWK_MODULE(ShowerReco);

}
