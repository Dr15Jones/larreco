////////////////////////////////////////////////////////////////////////
//
// LineMerger class
//
// maddalena.antonello@lngs.infn.it
// ornella.palamara@lngs.infn.it
// biagio.rossi@lhep.unibe.ch
// msoderbe@syr.edu
// joshua.spitz@yale.edu
//
// This algorithm is designed to merge 2D lines with similar slope and endpoints 
//  
////////////////////////////////////////////////////////////////////////

#include <string>
#include <cmath> // std::abs(), std::sqrt()
#include <iomanip>
#include <vector>
#include <array>
#include <memory> // std::unique_ptr<>
#include <utility> // std::move()


//Framework includes:
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h" 
#include "art/Framework/Core/FindManyP.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Persistency/Common/Ptr.h"
#include "art/Persistency/Common/PtrVector.h"

//LArSoft includes:
#include "SimpleTypesAndConstants/geo_types.h"
#include "RecoBase/Cluster.h"
#include "RecoBase/Hit.h"
#include "Utilities/AssociationUtil.h"
#include "RecoAlg/ClusterRecoUtil/StandardClusterParamsAlg.h"
#include "RecoAlg/ClusterParamsImportWrapper.h"
#include "ClusterFinder/ClusterCreator.h"



//#ifndef LINEMERGER_H
//#define LINEMERGER_H


namespace cluster {
   
  class LineMerger : public art::EDProducer {
    
  public:
    
    explicit LineMerger(fhicl::ParameterSet const& pset); 
    ~LineMerger();
    
    void produce(art::Event& evt);
    void beginJob();
    
  private:
        
    std::string     fClusterModuleLabel;
    double          fSlope; // tolerance for matching angles between two lines (in units of radians) 
    double          fEndpointWindow; // tolerance for matching endpoints (in units of time samples) 
   
    bool SlopeCompatibility(double slope1,double slope2);
    int  EndpointCompatibility(
      float sclstartwire, float sclstarttime,
      float sclendwire,   float sclendtime,
      float cl2startwire, float cl2starttime,
      float cl2endwire,   float cl2endtime
      );
    
  protected: 
    
  }; // class LineMerger

}

//#endif // LINEMERGER_H



namespace cluster{

  //-------------------------------------------------
  LineMerger::LineMerger(fhicl::ParameterSet const& pset) 
    : fClusterModuleLabel(pset.get<std::string>("ClusterModuleLabel"))
    , fSlope             (pset.get<double     >("Slope"))
    , fEndpointWindow    (pset.get<double     >("EndpointWindow"))
  {
    produces< std::vector<recob::Cluster> >();
    produces< art::Assns<recob::Cluster, recob::Hit> >();
  }

  //-------------------------------------------------
  LineMerger::~LineMerger()
  {
  }

  //-------------------------------------------------
  void LineMerger::beginJob()
  {
    //this doesn't do anything now, but it might someday
  }
    
  //------------------------------------------------------------------------------------//
  void LineMerger::produce(art::Event& evt)
  { 
    // Get a Handle for the input Cluster object(s).
    art::Handle< std::vector<recob::Cluster> > clusterVecHandle;
    evt.getByLabel(fClusterModuleLabel,clusterVecHandle);

    constexpr size_t nViews = 3; // number of views we map
    
    //one vector for each view in the geometry (holds the index of the cluster)
    std::array< std::vector<size_t>, nViews > ClsIndices;

    //vector with indicators for whether a cluster has been merged already
    std::array< std::vector<int>, nViews > Cls_matches;

    // loop over the input Clusters
    for(size_t i = 0; i < clusterVecHandle->size(); ++i){
      
      //get a art::Ptr to each Cluster
      art::Ptr<recob::Cluster> cl(clusterVecHandle, i);
      
      size_t view = 0;
      switch(cl->View()){
      case geo::kU :
        view = 0;
        break;
      case geo::kV :
        view = 1;
        break;
      case geo::kZ :
        view = 2;
        break;
      default :
        continue; // ignore this cluster and process the next one
      }// end switch on view
      
      Cls_matches[view].push_back(0);
      ClsIndices[view].push_back(i);
    }// end loop over input clusters

    std::unique_ptr<std::vector<recob::Cluster> >             SuperClusters(new std::vector<recob::Cluster>);
    std::unique_ptr< art::Assns<recob::Cluster, recob::Hit> > assn(new art::Assns<recob::Cluster, recob::Hit>);

    // prepare the algorithm to compute the cluster characteristics;
    // we use the "standard" one here; configuration would happen here,
    // but we are using the default configuration for that algorithm
    ClusterParamsImportWrapper<StandardClusterParamsAlg> ClusterParamAlgo;
    
    art::FindManyP<recob::Hit> fmh(clusterVecHandle, evt, fClusterModuleLabel);
    
    for(size_t i = 0; i < nViews; ++i){

      int clustersfound = 0; // how many merged clusters found in each plane
      int clsnum1       = 0;

      for(size_t c = 0; c < ClsIndices[i].size(); ++c){
        if(Cls_matches[i][clsnum1] == 1){
          ++clsnum1;
          continue;
        }

        // make a new cluster to put into the SuperClusters collection 
        // because we want to be able to adjust it later
        recob::Cluster cl1( clusterVecHandle->at(ClsIndices[i][c]) ); 
        
        // find the hits associated with the current cluster
        std::vector< art::Ptr<recob::Hit> > ptrvs = fmh.at(ClsIndices[i][c]); // copy

        Cls_matches[i][clsnum1] = 1; 
        ++clustersfound;
        
        int clsnum2 = 0;
        for(size_t c2 = 0; c2 < ClsIndices[i].size(); ++c2){

          if(Cls_matches[i][clsnum2] == 1){
            ++clsnum2;
            continue;
          }

          const recob::Cluster& cl2( clusterVecHandle->at(ClsIndices[i][c2]) );

          // find the hits associated with this second cluster
          std::vector< art::Ptr<recob::Hit> > ptrvs2 = fmh.at(ClsIndices[i][c2]); // copy
          
          // check that the slopes are the same
          // added 13.5 ticks/wirelength in ArgoNeuT. 
          // \todo need to make this detector agnostic
          // would be nice to have a LArProperties function that returns ticks/wire.
          bool sameSlope = SlopeCompatibility(cl1.StartAngle(), cl2.EndAngle())
            || SlopeCompatibility(cl1.EndAngle(), cl2.StartAngle());
          
          // check that the endpoints fall within a circular window of each other 
          // done in place of intercept matching
          int sameEndpoint = EndpointCompatibility(
            cl1.StartWire(), cl1.StartTick(),
            cl1.EndWire(),   cl1.EndTick(),
            cl2.StartWire(), cl2.StartTick(),
            cl2.EndWire(),   cl2.EndTick()
            );
          
          // if the slopes and end points are the same, combine the clusters
          // note that after 1 combination cl1 is no longer what we started 
          // with
          if(sameSlope && sameEndpoint){
            cl1 = cl1 + cl2;
            Cls_matches[i][clsnum2] = 1;

            // combine the hit collections
            // take into account order when merging hits from two clusters: doc-1776
            if (sameEndpoint == 1)
              ptrvs.insert(ptrvs.begin(), ptrvs2.begin(), ptrvs2.end());
            else if (sameEndpoint == -1){
              ptrvs2.insert(ptrvs2.begin(), ptrvs.begin(), ptrvs.end());
              ptrvs = std::move(ptrvs2);
            }
          }
          
          ++clsnum2;
        }// end loop over second cluster iterator

        // now add the final version of cl1 to the collection of SuperClusters
        // and create the association between the super cluster and the hits
        SuperClusters->push_back(std::move(cl1));
        util::CreateAssn(*this, evt, *(SuperClusters.get()), ptrvs, *(assn.get()));        
        ++clsnum1;

      }// end loop over first cluster iterator
    }// end loop over planes

    mf::LogVerbatim("Summary") << std::setfill('-') << std::setw(175) << "-" << std::setfill(' ');
    mf::LogVerbatim("Summary") << "LineMerger Summary:";
    for(size_t i = 0; i < SuperClusters->size(); ++i) 
      mf::LogVerbatim("Summary") << SuperClusters->at(i);

    evt.put(std::move(SuperClusters));
    evt.put(std::move(assn));

    return;

  }

  //------------------------------------------------------------------------------------//
  //checks the difference between angles of the two lines
  bool LineMerger::SlopeCompatibility(double slope1, double slope2)
  { 
    double sl1 = atan(slope1);
    double sl2 = atan(slope2);

    //the units of fSlope are radians
    bool comp  = std::abs(sl1-sl2) < fSlope ? true : false;

    return comp;
  }
  //------------------------------------------------------------------------------------//
  int LineMerger::EndpointCompatibility(
    float sclstartwire, float sclstarttime,
    float sclendwire,   float sclendtime,
    float cl2startwire, float cl2starttime,
    float cl2endwire,   float cl2endtime
  ) {

    /// \todo 13.5 ticks/wire. need to make this detector agnostic--spitz
    float distance = std::sqrt((pow(sclendwire-cl2startwire,2)*13.5) + pow(sclendtime-cl2starttime,2));

    //not sure if this line is necessary--spitz
    float distance2 = std::sqrt((pow(sclstartwire-cl2endwire,2)*13.5) + pow(sclstarttime-cl2endtime,2));
    
//    bool comp = (distance  < fEndpointWindow ||
//                 distance2 < fEndpointWindow) ? true : false;

    //determine which way the two clusters should be merged. TY
    int comp = 0;
    if (distance < fEndpointWindow) 
      comp = 1;
    else if (distance2 < fEndpointWindow)
      comp = -1;
    return comp;
  }



} // end namespace





namespace cluster{

  DEFINE_ART_MODULE(LineMerger)
  
} // end namespace 

