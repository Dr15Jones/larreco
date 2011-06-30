////////////////////////////////////////////////////////////////////////
//
// MuonFilter class:
// 
//
// pagebri3@msu.edu
//
////////////////////////////////////////////////////////////////////////
#ifndef MUONFILTER_H
#define MUONFILTER_H
#include "art/Framework/Core/EDFilter.h"
#include "TH2D.h"

namespace filt {

 class MuonFilter : public art::EDFilter  {
    
  public:
    
    explicit MuonFilter(fhicl::ParameterSet const& ); 
    virtual ~MuonFilter();      
    
    bool filter(art::Event& evt);
    void beginJob();
    void endJob();
    void Swap(int & x, int & y);
    void Swap(double & x, double & y);
    void reconfigure(fhicl::ParameterSet p);

  private:  
   
  protected:
 
    std::string fClusterModuleLabel;
    std::string fLineModuleLabel;
    std::vector<double>  fCuts; 
    double fDCenter; 
    double fDelay;
    double fTolerance;
    double  fMaxIon;
    double fIonFactor;
  
  
  }; // class MuonFilter

}

#endif // MUONFILTER_H
