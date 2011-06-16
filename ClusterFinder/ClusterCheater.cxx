////////////////////////////////////////////////////////////////////////
/// \file    ClusterCheater.cxx
/// \brief   create perfectly reconstructed clusters
/// \version $Id: GeometryTest.cxx,v 1.1 2011/02/17 01:45:48 brebel Exp $
/// \author  brebel@fnal.gov
////////////////////////////////////////////////////////////////////////
#include <vector>

// ROOT includes

// LArSoft includes
#include "Geometry/geo.h"
#include "MCCheater/BackTracker.h"
#include "MCCheater/ClusterCheater.h"
#include "RecoBase/recobase.h"
#include "Simulation/sim.h"
#include "Simulation/SimListUtils.h"
#include "RecoBase/HoughLineService.h"

// Framework includes
#include "art/Framework/Core/Event.h"
#include "fhiclcpp/ParameterSet.h"
#include "art/Persistency/Common/Handle.h"
#include "art/Persistency/Common/OrphanHandle.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "art/Framework/Core/TFileDirectory.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

namespace cheat{

  //--------------------------------------------------------------------
  ClusterCheater::ClusterCheater(fhicl::ParameterSet const& pset)
  {
    this->reconfigure(pset);

    produces< std::vector<recob::Cluster> >();
  }

  //--------------------------------------------------------------------
  ClusterCheater::~ClusterCheater()
  {
  }

  //--------------------------------------------------------------------
  void ClusterCheater::reconfigure(fhicl::ParameterSet pset)
  {
    fHitModuleLabel    = pset.get< std::string >("HitModuleLabel",    "ffthit"  );
    fG4ModuleLabel     = pset.get< std::string >("G4ModuleLabel",     "largeant");
    fDetSimModuleLabel = pset.get< std::string >("DetSimModuleLabel", "daq"     );

    return;
  }

  //--------------------------------------------------------------------
  void ClusterCheater::produce(art::Event& evt)
  {

    // grab the sim::ParticleList
    sim::ParticleList plist = sim::SimListUtils::GetParticleList(evt, fG4ModuleLabel);

    // print the list of particles first
    mf::LogInfo("ClusterCheater") << plist;

    // grab the hits that have been reconstructed
    art::Handle< std::vector<recob::Hit> > hitcol;
    evt.getByLabel(fHitModuleLabel, hitcol);

    // make a vector of them - we aren't writing anything out to a file
    // so no need for a art::PtrVector here
    std::vector< art::Ptr<recob::Hit> > hits;
    art::fill_ptr_vector(hits, hitcol);
    
    // get the sim::SimChannels as well
    art::Handle< std::vector<sim::SimChannel> > sccol;
    evt.getByLabel(fDetSimModuleLabel, sccol);
    
    std::vector< art::Ptr<sim::SimChannel> > scs;
    art::fill_ptr_vector(scs, sccol);

    // loop over the hits and figure out which particle contributed to each one
    std::vector< art::Ptr<recob::Hit> >::iterator itr = hits.begin();

    // adopt an EmEveIdCalculator to find the eve ID.  
    // will return a primary particle if it doesn't find 
    // a responsible particle for an EM process
    plist.AdoptEveIdCalculator(new sim::EmEveIdCalculator);

    // make a map of vectors of art::Ptrs keyed by eveID values
    std::map< int, std::vector< art::Ptr<recob::Hit> > > eveHitMap;
    std::map< int, std::vector< art::Ptr<recob::Hit> > >::iterator hitMapItr = eveHitMap.begin();

    // loop over all hits and fill in the map
    while( itr != hits.end() ){

      std::vector<cheat::TrackIDE> eveides = cheat::BackTracker::HitToEveID(plist, scs, *itr);

      // loop over all eveides for this hit
      for(std::vector<cheat::TrackIDE>::iterator eitr = eveides.begin(); eitr != eveides.end(); eitr++){

	// don't worry about eve particles that contribute less than 10% of the
	// energy in the current hit
	if( (*eitr).energyFrac < 0.1) continue;
	  
	hitMapItr = eveHitMap.find( (*eitr).trackID );
	
	// is this id already in the map, if so extend the collection 
	// by one hit, otherwise make a new collection and put it in
	// the map
	if( hitMapItr != eveHitMap.end() ){
	  ((*hitMapItr).second).push_back((*itr));
	}
	else{
	  std::vector< art::Ptr<recob::Hit> > hitvec;
	  hitvec.push_back(*itr);
	  eveHitMap[(*eitr).trackID] = hitvec;
	}

      } // end loop over eve IDs for this hit

      itr++;
    }// end loop over hits

    art::ServiceHandle<geo::Geometry> geo;

    // loop over the map and make clusters
    std::auto_ptr< std::vector<recob::Cluster> > clustercol(new std::vector<recob::Cluster>);

    unsigned int plane = 0;
    unsigned int wire  = 0;
    unsigned int tpc   = 0;
    for(hitMapItr = eveHitMap.begin(); hitMapItr != eveHitMap.end(); hitMapItr++){

      // separate out the hits for each particle into the different views
      std::vector< art::Ptr<recob::Hit> > eveHits( (*hitMapItr).second );

      for(size_t t = 0; t < geo->NTPC(); ++t){
	for(size_t pl = 0; pl < geo->Nplanes(t); ++pl){
	  art::PtrVector<recob::Hit> ptrvs;
	  double startWire = 1.e6;
	  double startTime = 1.e6;
	  double endWire   = -1.e6;
	  double endTime   = -1.e6;
	  double dTdW      = 0.;
	  double dQdW      = 0.;

	  for(size_t h = 0; h < eveHits.size(); ++h){

	    geo->ChannelToWire(eveHits[h]->Channel(), tpc, plane, wire);

	    if(plane != pl && tpc != t) continue;
	  
	    ptrvs.push_back(eveHits[h]);

	    if(wire < startWire){
	      startWire = wire;
	      startTime = 1.e6;
	    }
	    if(wire > endWire  ){
	      endWire = wire;
	      endTime = -1.e-6;
	    }
	    
	    if(wire == startWire && eveHits[h]->StartTime() < startTime) startTime = eveHits[h]->StartTime();
	    if(wire == endWire   && eveHits[h]->EndTime()   > endTime  ) endTime   = eveHits[h]->EndTime();

	  } // end loop over hits for this particle	

	  // figure out the rest of the cluster information using these hits
	  
	  // use the HoughLineService to get dTdW for these hits
	  art::ServiceHandle<cluster::HoughLineService> hls;
	  double intercept = 0.;
	  hls->Transform(eveHits, dTdW, intercept);

	  // \todo now figure out the dQdW

	  // add a cluster to the collection.  Make the ID be the eve particle
	  // trackID*1000 + plane number*100 + tpc that the current hits are from
	  clustercol->push_back(recob::Cluster(ptrvs, 
					       startWire, 0.,
					       startTime, 0.,
					       endWire,   0.,
					       endTime,   0.,
					       dTdW,      0.,
					       dQdW,      0.,
					       ((*hitMapItr).first * 1000) + pl*100 + tpc));

	  mf::LogInfo("ClusterCheater") << "adding cluster: \n" 
					<< clustercol->back()
					<< "\nto collection.";


	} // end loop over the number of planes
      } // end loop over the tpcs
    } // end loop over the map

    evt.put(clustercol);

    return;

  } // end produce

} // end namespace
