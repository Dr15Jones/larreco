////////////////////////////////////////////////////////////////////////
//
// EmptyFilter class
//
// pagebri3@msu.edu
//
////////////////////////////////////////////////////////////////////////

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
}
#include <math.h>
#include <algorithm>
#include <iostream>
#include <fstream>

//Framework Includes
#include "FWCore/Framework/interface/Event.h" 
#include "FWCore/ParameterSet/interface/ParameterSet.h" 
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h" 
#include "DataFormats/Common/interface/Handle.h" 
#include "DataFormats/Common/interface/Ptr.h" 
#include "DataFormats/Common/interface/PtrVector.h" 
#include "FWCore/Framework/interface/MakerMacros.h" 
#include "FWCore/ServiceRegistry/interface/Service.h" 
#include "FWCore/Services/interface/TFileService.h" 
#include "FWCore/Framework/interface/TFileDirectory.h" 
#include "FWCore/MessageLogger/interface/MessageLogger.h" 
#include "FWCore/ServiceRegistry/interface/ServiceMaker.h" 


//Larsoft Includes
#include "EmptyFilter.h"
#include "RecoBase/recobase.h"
#include "Geometry/geo.h"
#include "Geometry/WireGeo.h"

namespace filt{

  //-------------------------------------------------
  EmptyFilter::EmptyFilter(edm::ParameterSet const & pset) : 
    fHitsModuleLabel(pset.getParameter< std::string > ("HitsModuleLabel")), 
    fMinNumHits(pset.getParameter< int > ("MinHits")),  
    fMinIonization(pset.getParameter< double > ("MinIonization"))
  {   
  }

  //-------------------------------------------------
  EmptyFilter::~EmptyFilter()
  {
  }

  void EmptyFilter::beginJob(const edm::EventSetup&)
  {
    return;
  }

  //-------------------------------------------------
  bool EmptyFilter::filter(edm::Event &evt, edm::EventSetup const&)
  { 
    edm::Service<edm::TFileService> tfs;
     totHitHist = tfs->make<TH1I>("totHitHist","Hit Number Per Event",750,0,1500);
    totIonSelHist = tfs->make<TH2D>("totIonSelHist","Ionization Per selected Event",500,0,20000,500,0,20000);
    totIonRejHist = tfs->make<TH2D>("totIonRejHist","Ionization Per rejected Event",500,0,20000,500,0,20000);
    selHitHist= tfs->make<TH1I>("selHitHist","Hit Number Per selected  Event",750, 0 ,1500);
    rejHitHist= tfs->make<TH1I>("rejHitHist","Hit Number Per rejected Event",750, 0 ,1500);
    numEventHist = tfs->make<TH1I>("numEventHist","Number of Events Processed and Selected",2,0,2);
    resultTable = tfs->make<TH2I>("resultTable","Event number is x axis, y axis bins 0=selected,1= hit num, 2= one plane empty, 3= too little ionization",40000,0,40000,4,0,4);
    numEventHist->Fill(0);
    int failFlag = 0;
    double indIon(0.0), colIon(0.0);
    int event = evt.id().event();
    unsigned int chan(0), wire(0), plane(0); 
    
    

    edm::Service<geo::Geometry> geom;
    edm::Handle< std::vector<recob::Hit> > hitHandle;
    evt.getByLabel(fHitsModuleLabel,hitHandle);
    edm::PtrVector<recob::Hit> hitvec;
    for(unsigned int i = 0; i < hitHandle->size(); ++i){
      edm::Ptr<recob::Hit> prod(hitHandle, i);
      hitvec.push_back(prod);
    }

    int numHits = hitvec.size();
    if( numHits> 0) {
      totHitHist->Fill(numHits);
      if(numHits < fMinNumHits) {
	std::cout << "Too few hits"<< std::endl; 
	failFlag=1;
      }  
      if(failFlag==0) {
	chan = hitvec[0]->Wire()->RawDigit()->Channel();
	geom->ChannelToWire(chan,plane,wire);
	//Check to see if either plane is empty
	if(plane == 1){ 
	  std::cout << "Induction empty." << std::endl;
	  failFlag=2;
	}
	unsigned int j(0);  
	unsigned int colStart(0);  //first Collection plane index
	//advances j to collection plane
	while(plane ==0) {
	  indIon+=hitvec[j]->MIPs();
	  j++;
	  if(j == hitvec.size()) {
	    failFlag=2;
	    std::cout << "Collection empty." << std::endl;
	    plane = 1;
	  }
	  else{
	    chan = hitvec[j]->Wire()->RawDigit()->Channel();
	    geom->ChannelToWire(chan,plane,wire);
	  }
	}
	colStart = j;
	for(; j < hitvec.size(); j++){
	  colIon+=hitvec[j]->MIPs();
	}
	double minIon=0;
	if((1.92*indIon)>colIon) minIon = colIon;
	else minIon=1.92*indIon;
	std::cout <<"min ionization " << minIon <<std::endl;
	if (minIon < fMinIonization) failFlag=3;
      }
    }
    else failFlag = 1; 
    resultTable->Fill(event,failFlag);   
    if(failFlag>0){
      totIonRejHist->Fill(indIon,colIon);
      rejHitHist->Fill(numHits); 
      return  false;
    }
    numEventHist->Fill(1);
    totIonSelHist->Fill(indIon,colIon);
    selHitHist->Fill(numHits);
  
    return true;
  }
	      
} //end namespace
