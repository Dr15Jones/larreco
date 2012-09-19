////////////////////////////////////////////////////////////////////////
// $Id: HoughLineFinderAna.cxx,v 1.36 2010/09/15  bpage Exp $
//
// \file HoughLineFinder.h
//
// \author josh
//
////////////////////////////////////////////////////////////////////////
#ifndef CLUSTER_HOUGHLINEFINDER_H
#define CLUSTER_HOUGHLINEFINDER_H

#include "TMath.h"
#include <vector>
#include <string>

#include "art/Framework/Core/EDProducer.h"
#include "ClusterFinder/HoughBaseAlg.h"
class TH1F;
class TTree;

namespace cluster {
   
  class HoughLineFinder : public art::EDProducer {
    
  public:
    
    explicit HoughLineFinder(fhicl::ParameterSet const& pset); 
    virtual ~HoughLineFinder();

    void reconfigure(fhicl::ParameterSet const& p);
         
    void produce(art::Event& evt);
     
    
  private:

    std::string fDBScanModuleLabel;    
    
    HoughBaseAlg fHLAlg;            ///< object that does the Hough Transform
  
  };
  
  
}



#endif // CLUSTER_HOUGHLINEFINDER_H
