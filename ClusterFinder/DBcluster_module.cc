////////////////////////////////////////////////////////////////////////
// $Id: DBSCANfinderAna.cxx,v 1.36 2010/09/15  bpage Exp $
//
// \file DBcluster_module.cc
//
// \author kinga.partyka@yale.edu
//
////////////////////////////////////////////////////////////////////////

//Framework includes:
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Persistency/Common/Ptr.h"
#include "art/Persistency/Common/PtrVector.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "art/Framework/Services/Optional/TFileDirectory.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "art/Framework/Core/EDProducer.h"

//LArSoft includes
#include "Geometry/Geometry.h"
#include "Geometry/CryostatGeo.h"
#include "Geometry/TPCGeo.h"
#include "Geometry/PlaneGeo.h"
#include "RecoBase/Cluster.h"
#include "RecoBase/Hit.h"
#include "Utilities/AssociationUtil.h"
#include "Filters/ChannelFilter.h"
#include "ClusterFinder/DBScanAlg.h"

#include <fstream>
#include <cstdlib>
#include "TGeoManager.h"
#include "TH1.h"
#include <vector>
#include <cmath>
#include <iostream>
#include <iomanip>

class TH1F;

namespace cluster{
   
  //--------------------------------------------------------------- 
  class DBcluster : public art::EDProducer
  {
  public:
    explicit DBcluster(fhicl::ParameterSet const& pset); 
    ~DBcluster();
    void produce(art::Event& evt);
    void beginJob();
    void reconfigure(fhicl::ParameterSet const& p);
    
  private:
       
    TH1F *fhitwidth;
    TH1F *fhitwidth_ind_test;  
    TH1F *fhitwidth_coll_test;  
   
    std::string fhitsModuleLabel;
   
    DBScanAlg fDBScan; ///< object that implements the DB scan algorithm
  };

}

namespace cluster{

  //-------------------------------------------------
  DBcluster::DBcluster(fhicl::ParameterSet const& pset)
    : fDBScan(pset.get< fhicl::ParameterSet >("DBScanAlg")) 
  {  
    this->reconfigure(pset);
    produces< std::vector<recob::Cluster> >();  
    produces< art::Assns<recob::Cluster, recob::Hit> >();
  }
  
  //-------------------------------------------------
  DBcluster::~DBcluster()
  {
  }
  
  //-------------------------------------------------
  void DBcluster::reconfigure(fhicl::ParameterSet const& p)
  {
    fhitsModuleLabel = p.get< std::string >("HitsModuleLabel");
    fDBScan.reconfigure(p.get< fhicl::ParameterSet >("DBScanAlg"));
  }
  
  //-------------------------------------------------
  void DBcluster::beginJob(){
    // get access to the TFile service
    art::ServiceHandle<art::TFileService> tfs;
  
    fhitwidth= tfs->make<TH1F>(" fhitwidth","width of hits in cm", 50000,0 ,5  );
    fhitwidth_ind_test= tfs->make<TH1F>("fhitwidth_ind_test","width of hits in cm", 50000,0 ,5  );
    fhitwidth_coll_test= tfs->make<TH1F>("fhitwidth_coll_test","width of hits in cm", 50000,0 ,5  );
      
  }
  
  //-----------------------------------------------------------------
  void DBcluster::produce(art::Event& evt)
  {
     
    //get a collection of clusters   
    std::unique_ptr<std::vector<recob::Cluster> > ccol(new std::vector<recob::Cluster>);
    std::unique_ptr< art::Assns<recob::Cluster, recob::Hit> > assn(new art::Assns<recob::Cluster, recob::Hit>);
  
    art::ServiceHandle<geo::Geometry> geom;
  
    art::Handle< std::vector<recob::Hit> > hitcol;
    evt.getByLabel(fhitsModuleLabel,hitcol);
    
    // loop over all hits in the event and look for clusters (for each plane)
    art::PtrVector<recob::Hit> allhits;
  
    // get the ChannelFilter
    filter::ChannelFilter chanFilt;
        
    for(unsigned int cstat = 0; cstat < geom->Ncryostats(); ++cstat){
      for(unsigned int tpc = 0; tpc < geom->Cryostat(cstat).NTPC(); ++tpc){
        for(unsigned int plane = 0; plane < geom->Cryostat(cstat).TPC(tpc).Nplanes(); ++plane){
  	geo::SigType_t sigType = geom->Cryostat(cstat).TPC(tpc).Plane(plane).SignalType();
  	for(size_t i = 0; i< hitcol->size(); ++i){
        
  	  art::Ptr<recob::Hit> hit(hitcol, i);
        
  	  if(hit->WireID().Plane    == plane && 
	     hit->WireID().TPC      == tpc   && 
	     hit->WireID().Cryostat == cstat) allhits.push_back(hit);
  	
  	}  
        
  	fDBScan.InitScan(allhits, chanFilt.SetOfBadChannels());
  
  	//----------------------------------------------------------------
  	for(unsigned int j = 0; j < fDBScan.fps.size(); ++j){
  	  
  	  if(allhits.size() != fDBScan.fps.size()) break;
  	  
  	  fhitwidth->Fill(fDBScan.fps[j][2]);
  	  
  	  if(sigType == geo::kInduction)  fhitwidth_ind_test->Fill(fDBScan.fps[j][2]);
  	  if(sigType == geo::kCollection) fhitwidth_coll_test->Fill(fDBScan.fps[j][2]);
  	}
   
  	//*******************************************************************
  	fDBScan.run_cluster();
  	
  	for(size_t i = 0; i < fDBScan.fclusters.size(); ++i){
  	  art::PtrVector<recob::Hit> clusterHits;
  	  double totalQ = 0.;
  
  	  for(size_t j = 0; j < fDBScan.fpointId_to_clusterId.size(); ++j){	  
  	    if(fDBScan.fpointId_to_clusterId[j]==i){
  	      clusterHits.push_back(allhits[j]);
  	      totalQ += clusterHits.back()->Charge();
  	    }
  	  }
           
  	  ////////
  	  if (clusterHits.size()>0){
  	    
  	    /// \todo: need to define start and end positions for this cluster and slopes for dTdW, dQdW
  	    unsigned int sw = clusterHits[0]->WireID().Wire;
  	    unsigned int ew = clusterHits[clusterHits.size()-1]->WireID().Wire;
  	 
  	    recob::Cluster cluster(sw*1., 0.,
  				   clusterHits[0]->PeakTime(), clusterHits[0]->SigmaPeakTime(),
  				   ew*1., 0.,
  				   clusterHits[clusterHits.size()-1]->PeakTime(), clusterHits[clusterHits.size()-1]->SigmaPeakTime(),
  				   -999., 0., 
  				   -999., 0.,
  				   totalQ,
  				   geom->View(clusterHits[0]->Channel()),
  				   ccol->size());
  	    
  	    ccol->push_back(cluster);
  
  	    // associate the hits to this cluster
  	    util::CreateAssn(*this, evt, *(ccol.get()), clusterHits, *(assn.get()));
  	    
  	    clusterHits.clear();
  	    
  	  }//end if clusterHits has at least one hit
     
  	}//end loop over fclusters
  	
  	allhits.clear();
        } // end loop over planes
      } // end loop over tpcs
    } // end loop over cryostats
  
    mf::LogVerbatim("Summary") << std::setfill('-') << std::setw(175) << "-" << std::setfill(' ');
    mf::LogVerbatim("Summary") << "DBcluster Summary:";
    for(unsigned int i = 0; i<ccol->size(); ++i) mf::LogVerbatim("Summary") << ccol->at(i) ;
  
    evt.put(std::move(ccol));
    evt.put(std::move(assn));
  
    return;
  }
  
  
} // end namespace 
  
  



namespace cluster{

  DEFINE_ART_MODULE(DBcluster);
  
} 

