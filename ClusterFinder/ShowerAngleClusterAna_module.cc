#include "art/Framework/Core/ModuleMacros.h"
//#include "ClusterFinder/ShowerAngleClusterAna.h"
////////////////////////////////////////////////////////////////////////
/// \file  ShowerAngleClusterAna.h
/// \brief
///
///
/// \version $Id: SingleGen.cxx,v 1.4 2010/03/29 09:54:01 brebel Exp $
/// \author:  andrzejs
////////////////////////////////////////////////////////////////////////
#ifndef SHOWERANGLECLUSTERANA_H
#define SHOWERANGLECLUSTERANA_H

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
}


#include <vector>
#include <string>
#include <math.h>
#include <algorithm>
#include <iostream>
#include <fstream>


// LArSoft includes

#include "Simulation/SimListUtils.h"
#include "SimulationBase/MCTruth.h"
#include "Utilities/AssociationUtil.h"
#include "Geometry/PlaneGeo.h"
#include "CLHEP/Random/JamesRandom.h"
#include "Utilities/SeedCreator.h"
#include "RecoBase/Hit.h"
#include "RecoBase/Cluster.h"
#include "Utilities/LArProperties.h"
#include "Utilities/GeometryUtilities.h"
#include "Utilities/DetectorProperties.h"
#include "RecoAlg/ClusterParamsAlg.h"
#include "MCCheater/BackTracker.h"

// Framework includes


#include "art/Framework/Core/EDAnalyzer.h" // include the proper bit of the framework
#include "art/Framework/Services/Registry/ActivityRegistry.h"
#include "art/Framework/Principal/Event.h" 
#include "fhiclcpp/ParameterSet.h" 
#include "art/Framework/Principal/Handle.h" 
#include "art/Persistency/Common/Ptr.h" 
#include "art/Persistency/Common/PtrVector.h" 
#include "art/Framework/Services/Registry/ServiceHandle.h" 
#include "art/Framework/Services/Optional/TFileService.h" 
#include "art/Framework/Services/Optional/TFileDirectory.h" 
#include "messagefacility/MessageLogger/MessageLogger.h" 

#include "TH2F.h"
#include "TF1.h"
#include "TFile.h"
#include "TMath.h"
#include "TTree.h"
#include "TLine.h"
#include "TH1F.h"
#include "TGraph.h"
#include "TPrincipal.h"




// ***************** //






namespace cluster {

  class ShowerAngleClusterAna : public art::EDAnalyzer {
    
  public:

    /**METHODS global*/
    explicit ShowerAngleClusterAna(fhicl::ParameterSet const& pset);/**Constructor*/
    virtual ~ShowerAngleClusterAna();                               /**Destructor*/
    void beginJob();                                     
    void beginRun(const art::Run& run);
    void reconfigure(fhicl::ParameterSet const& pset);
    void analyze(const art::Event& evt);                       /**Routine that finds the cluster and sets the dTdW of the 2D shower*/
   
 
    void Get2DVariables(unsigned int nClust,std::vector < art::Ptr < recob::Hit> > hitlist);   /** Calculate 2D variables to be saved into 	  */ 
 
    void GetVertexCluster(std::vector < art::Ptr < recob::Hit> > hitlist,int iClust);
    
    void HitsPurity(std::vector< art::Ptr<recob::Hit> > const& hits, int& trackid, double& purity, double& maxe);
    
  private:

    ClusterParamsAlg fCParAlg;
    double fWiretoCm,fTimetoCm,fWireTimetoCmCm;
    
    std::vector< unsigned int >fNWires;
    double fNTimes;
    
     
    art::ServiceHandle<geo::Geometry> geom;
    art::ServiceHandle<util::DetectorProperties> detp;
    art::ServiceHandle<util::LArProperties> larp;
    art::ServiceHandle<geo::Geometry> geo;
    util::GeometryUtilities gser;
   
    float fTimeTick; // time sample in us
    float fPresamplings;
    float fDriftVelocity;
    const static int    alpha           = 5;     // parameter (how many RMs (of the anglular distribution) is large the cone of the shower)
   
   
    int fRun,fEvent,fSubRun;
    bool fForceRightGoing;

    
    std::vector< unsigned int > fMinWire,fMaxWire;
    std::vector < double > fMinTime,fMaxTime;
    
    std::vector< unsigned int > fMinWireHigh,fMaxWireHigh;
    std::vector < double > fMinTimeHigh,fMaxTimeHigh;
    
    
    //input parameter labels:
 
    std::string fClusterModuleLabel;
    std::string fVertexCLusterModuleLabel;
    std::string fMCGeneratorLabel;
    std::string fLarGeantlabel;
    int fUseMCVertex;
  
  
    // 2D slope and intercept of the shower axis
    std::vector <double> slope;       // in wire, time
    std::vector <double> slope_cm;       // in cm, cm
    std::vector <double> xangle;       // in cm, cm
   
 //   std::vector <double> lineslope;   // in wire, time
 //   std::vector <double>  lineinterc;   


    std::vector <double> lineslopetest;   // in wire, time
    std::vector <double>  lineinterctest;   

    
    double fWirePitch ;   // wire pitch in cm

  
    
    std::vector<std::vector<double> > fSingleEvtAngle;  // vector to show the plane omega distributions 
    std::vector<std::vector<double> > fSingleEvtAngleVal;  //vector to show the plane omega distributions


    std::vector<std::vector<double> > fShowerWidthProfile2D;  // vector to show the plane shower Width distribution 
    std::vector<std::vector<double> > fShowerChargeProfile2D;  //vector to show the plane shower Charge distribution
    std::vector<std::vector<double> > fShowerPosition2D;  //vector to store the positions of hit values stored in the previous two vectors.

    std::vector<unsigned int> fWireVertex;  // wire coordinate of vertex for each plane
    std::vector<double> fTimeVertex;  // time coordinate of vertex for each plane

    std::vector<unsigned int> fWireLast;  // wire coordinate of last point for each plane
    std::vector<double> fTimeLast;  // time coordinate of last point for each plane

    std::vector<unsigned int> fClusterPlane;  // time coordinate of last point for each plane
    
    std::vector<double>  test_wire_vertex,test_time_vertex;
    std::vector<double>  hough_wire_vertex,hough_time_vertex;

    std::vector<double> fRMS_wire;
    std::vector<double> fRMS_time;
    std::vector<double> fChisq;
    std::vector<double> fcorrelation;
    std::vector<double> fcovariance;

    std::vector<double> fOffAxis;
    std::vector<double> fOnAxis;
    std::vector<double> fOffAxisNorm; 
    std::vector<double> fOnAxisNorm; 
    std::vector<int> fNhits;
    std::vector<int> fNhitsClust;
    std::vector<double> fHitDensity;
    std::vector<double> fLength;
    
    std::vector<double> fOffAxisNormHD; 
    std::vector<double> fOnAxisNormHD; 
    std::vector<double> fLengthHD;
    std::vector<double> fPrincipalHD;
    std::vector<double> fOffAxisNormLength; 
    std::vector<double> slope2DHD;
    std::vector<int>    fMultiHitWires;
    std::vector<double> fMultiHitWiresNorm;
    std::vector<double> fOffAxisOnlyOff; 
    std::vector<double> fOffAxisOnlyOffNorm; 
    
    std::vector<double> fHitDensityOA;
    std::vector<double> fOnAxisNormOA; 
    std::vector<double> fLengthOA;
    
     
    std::vector <double> fMeanCharge;       // in wire, time
    std::vector<int> fDirection;
    std::vector<int> fHoughDirection;
    std::vector<int> fPrincDirection;
    std::vector<int> fTrunkDirection;
    std::vector<int> fMultiHitDirection;
    std::vector<int> fHitDensityDirection;
    
    std::vector <double> fTotalCharge;       // in wire, time
    std::vector <double> fMaxweight;       // in wire, time
    std::vector <double> fSideChargeStart;       // in wire, time
    std::vector <double> fSideChargeEnd;       // in wire, time
    std::vector<double>  TrunkWVertex,TrunkTVertex;
    std::vector<double>  TrunkWVertexwght,TrunkTVertexwght;
    std::vector<int> lochitlistsize;
    std::vector<int> lochitlistendsize;
    std::vector<int> nhoughlines;
    std::vector<int> nhoughlinesend;
    std::vector<double> fHighBin;
    std::vector<double> fLowBin;
    std::vector<double> fReHighBin;
    std::vector<double> fReLowBin;
    std::vector<double> fVerticalness;
    // unsigned int tpc;    //tpc type
    unsigned int fNPlanes; // number of planes  

    TH1F *  fh_omega_single;
   
    
    TH1F * fRecoWireHist;
    TH1F *fRecoTimeHist; 
    
    TTree* ftree_cluster;

     void CalculateAxisParameters(unsigned nClust, std::vector < art::Ptr < recob::Hit> >  hitlist,double wstart,double tstart,double wend,double tend);

     double Get2DAngleForHit( unsigned int wire, double time,std::vector < art::Ptr < recob::Hit> > hitlist);
     
     void ClearandResizeVectors(unsigned int nClusters);
   
    
    //MCInformation
    std::vector< int > mcpdg;
    std::vector < double > mcenergy;
    std::vector <double > mcphi;
    std::vector <double > mctheta;
    
    std::vector< unsigned int> mcwirevertex;  // wire coordinate of vertex for each plane
    std::vector< double> mctimevertex;  // time coordinate of vertex for each plane
    std::vector<double> mcx;
    std::vector<double> mcy;
    std::vector<double> mcz;
    std::vector<int> mcplane;
    std::vector<int> mcdirection;
    std::vector <double > mcomega;
    
    std::vector<bool> startflag;
    bool endflag;
    bool matchflag;
    
    
    TH2F * hithistlow_wires;
    TH2F * hithist_wires_high; 
    TH1F * hithistinv; 
    TH1F * hitinv2; 
    TH1F * hitreinv2; 
    
  }; // class ShowerAngleClusterAna

}

#endif // SHOWERANGLECLUSTER_H

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  start definitions
//////////////////////////////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
cluster::ShowerAngleClusterAna::ShowerAngleClusterAna(fhicl::ParameterSet const& pset)
 :  fCParAlg(pset.get< fhicl::ParameterSet >("ClusterParamsAlg"),pset.get< std::string >("module_type"))
{
  this->reconfigure(pset);
//   produces< std::vector<recob::Cluster> >();
//   produces< art::Assns<recob::Cluster, recob::Hit>  >(); 
//  // produces< art::Assns<recob::Cluster, recob::Hit>  >(); 
//   //produces< art::Assns<recob::Cluster, recob::Cluster>  >(); 
//   produces< std::vector < art::PtrVector <recob::Cluster> >  >();
  
    // Create random number engine needed for PPHT
  createEngine(SeedCreator::CreateRandomNumberSeed(),"HepJamesRandom");

  
}


void cluster::ShowerAngleClusterAna::reconfigure(fhicl::ParameterSet const& pset) 
{
  fClusterModuleLabel 		=pset.get< std::string >("ClusterModuleLabel");
  fCParAlg.reconfigure(pset.get< fhicl::ParameterSet >("ClusterParamsAlg"));
 }

// ***************** //
cluster::ShowerAngleClusterAna::~ShowerAngleClusterAna()
{
}

//____________________________________________________________________________
 void cluster::ShowerAngleClusterAna::beginRun(const art::Run& run)
  {
    
  //  std::cout << "------------- In SHowANgle preBeginRun"<<  larp->Efield() << std::endl;
  //  


    return;
  }




//-----------------------------------------------
namespace cluster {
struct SortByWire 
{
  bool operator() (recob::Hit const& h1, recob::Hit const& h2) const 
  { return 
      h1.Wire()->RawDigit()->Channel() < h2.Wire()->RawDigit()->Channel() ;
  }
};
}

// ***************** //
void cluster::ShowerAngleClusterAna::beginJob()
{

  //temporary:
  unsigned int tpc=0;
  

  
  // this will not change on a run per run basis.
  fNPlanes = geo->Nplanes();
  fWirePitch = geo->WirePitch(0,1,0);    //wire pitch in cm
  fTimeTick=detp->SamplingRate()/1000.; 
 
  /**Get TFileService and define output Histograms*/
  art::ServiceHandle<art::TFileService> tfs;

  
  fNTimes=geo->DetHalfWidth(tpc)*2/(fTimetoCm);
  fNWires.resize(fNPlanes);
    
  fRecoWireHist = tfs->make<TH1F>(Form("recowire"),"RecoWire",100,-100., 100.);
  fRecoTimeHist = tfs->make<TH1F>(Form("recotime"),"RecoTime",100,-100., 100.);
    
  for(unsigned int i=0;i<fNPlanes;++i){
   
     fNWires[i]=geo->Nwires(i); //Plane(i,tpc).Nwires();
     
 }  // end loop on planes
  
  
  
  
  
   ftree_cluster =tfs->make<TTree>("ShowerAngleClusterAna","Results");/**All-knowing tree with reconstruction information*/
   fh_omega_single= tfs->make<TH1F>("fh_omega_single","Theta distribution Hit",720,-180., 180.) ;
    
    ftree_cluster->Branch("run",&fRun,"run/I");
    ftree_cluster->Branch("subrun",&fSubRun,"subrun/I");
    ftree_cluster->Branch("event",&fEvent,"event/I");
    ftree_cluster->Branch("nplanes",&fNPlanes,"nplanes/I");
  
    
    ftree_cluster->Branch("mcpdg","std::vector< int>", &mcpdg);
    ftree_cluster->Branch("mcenergy","std::vector<double>", &mcenergy);
    ftree_cluster->Branch("mcphi","std::vector< double >", &mcphi);
    ftree_cluster->Branch("mctheta","std::vector<double >", &mctheta);
    ftree_cluster->Branch("mcomega","std::vector<double >", &mcomega);
    
    ftree_cluster->Branch("mcwirevertex","std::vector<unsigned int>", &mcwirevertex);
    ftree_cluster->Branch("mctimevertex","std::vector<double>", &mctimevertex);
    
    ftree_cluster->Branch("mcx","std::vector< double >", &mcx);
    ftree_cluster->Branch("mcy","std::vector<double >", &mcy);
    ftree_cluster->Branch("mcz","std::vector<double >", &mcz);
    ftree_cluster->Branch("mcplane","std::vector< int>", &mcplane);
    ftree_cluster->Branch("mcdirection","std::vector< int>", &mcdirection);
    
 
    
    
    
    ///////////// reconstructed quantities
    ftree_cluster->Branch("wire_vertex","std::vector<unsigned int>", &fWireVertex);
    ftree_cluster->Branch("time_vertex","std::vector<double>", &fTimeVertex);

    ftree_cluster->Branch("wire_last","std::vector<unsigned int>", &fWireLast);
    ftree_cluster->Branch("time_last","std::vector<double>", &fTimeLast);
    

      
    ftree_cluster->Branch("fDirection","std::vector<int>", &fDirection);
    ftree_cluster->Branch("fPrincDirection","std::vector<int>", &fPrincDirection);
    ftree_cluster->Branch("fHoughDirection","std::vector<int>", &fHoughDirection);
    ftree_cluster->Branch("fTrunkDirection","std::vector<int>", &fTrunkDirection);
    ftree_cluster->Branch("fMultiHitDirection","std::vector<int>", &fMultiHitDirection);
    ftree_cluster->Branch("fHitDensityDirection","std::vector<int>", &fHitDensityDirection);
    
    ftree_cluster->Branch("fOffAxis","std::vector<double>", &fOffAxis);
    ftree_cluster->Branch("fOnAxis","std::vector<double>", &fOnAxis);
    ftree_cluster->Branch("fOffAxisNorm","std::vector<double>", &fOffAxisNorm);
    ftree_cluster->Branch("fOnAxisNorm","std::vector<double>", &fOnAxisNorm);
    ftree_cluster->Branch("fNhits","std::vector<int>", &fNhits);
    ftree_cluster->Branch("fNhitsClust","std::vector<int>", &fNhitsClust);
    ftree_cluster->Branch("fHitDensity","std::vector<double>", &fHitDensity);
     ftree_cluster->Branch("fLength","std::vector<double>", &fLength);
    
     
    ftree_cluster->Branch("fOffAxisNormHD","std::vector<double>", &fOffAxisNormHD);
    ftree_cluster->Branch("fOnAxisNormHD","std::vector<double>", &fOnAxisNormHD);
    ftree_cluster->Branch("fLengthHD","std::vector<double>", &fLengthHD);
    
    ftree_cluster->Branch("fPrincipalHD","std::vector<double>", &fPrincipalHD);
    ftree_cluster->Branch("fOffAxisNormLength","std::vector<double>", &fOffAxisNormLength);
    ftree_cluster->Branch("slope2DHD","std::vector<double>", &slope2DHD);
    ftree_cluster->Branch("fMultiHitWires","std::vector<int>", &fMultiHitWires);
    ftree_cluster->Branch("fMultiHitWiresNorm","std::vector<double>", &fMultiHitWiresNorm);
    ftree_cluster->Branch("fOffAxisOnlyOff","std::vector<double>", &fOffAxisOnlyOff);
    ftree_cluster->Branch("fOffAxisOnlyOffNorm","std::vector<double>", &fOffAxisOnlyOffNorm);
    
     
    ftree_cluster->Branch("fOnAxisNormOA","std::vector<double>", &fOnAxisNormOA);
    ftree_cluster->Branch("fHitDensityOA","std::vector<double>", &fHitDensityOA);
    ftree_cluster->Branch("fLengthOA","std::vector<double>", &fLengthOA);
     
     

  }

// ************************************* //
void cluster::ShowerAngleClusterAna::ClearandResizeVectors(unsigned int nClusters) {
  
  ///////////////
  fMinWire.clear();
  fMaxWire.clear();
  fMinTime.clear();
  fMaxTime.clear();
  
  fMinWireHigh.clear();
  fMaxWireHigh.clear();
  fMinTimeHigh.clear();
  fMaxTimeHigh.clear();
  
  fVerticalness.clear();
    
  
  
  startflag.clear();
  
  ///////////////////
  
  fRMS_wire.clear();
  fRMS_time.clear();
  
  fChisq.clear();
  fcorrelation.clear();
  fcovariance.clear();
  
  lineslopetest.clear();
  lineinterctest.clear();
  
  /////
  fClusterPlane.clear();  // time coordinate of last point for each plane
  fHighBin.clear();
  fLowBin.clear();
  fReHighBin.clear();
  fReLowBin.clear(); 
  
  //fPitch.resize(0);  // Pitch calculated the old way
  fShowerWidthProfile2D.clear(); ;  // vector to show the plane shower Width distribution 
  fShowerChargeProfile2D.clear(); ;  //vector to show the plane shower Charge distribution
  fShowerPosition2D.clear(); ;  //vector to store the positions of hit values stored in the previous two vectors.
  fSingleEvtAngle.clear(); 
  fSingleEvtAngleVal.clear();

  fWireVertex.clear();
  fTimeVertex.clear();
  fWireLast.clear();
  fTimeLast.clear();
  
  slope.clear();
  xangle.clear();
  slope_cm.clear();
  
  
  test_wire_vertex.clear();
  test_time_vertex.clear();
// 
//   
  hough_wire_vertex.clear();
  hough_time_vertex.clear();
//   
  fMeanCharge.clear();       // in wire, time
  fDirection.clear();
  fPrincDirection.clear();
  fTrunkDirection.clear();
  fHoughDirection.clear();
  fMultiHitDirection.clear();
  fHitDensityDirection.clear();
  
  
  fTotalCharge.clear();       // in wire, time
  fMaxweight.clear();       // in wire, time
  fSideChargeStart.clear();       // in wire, time
  fSideChargeEnd.clear();       // in wire, time
  TrunkWVertex.clear();
  TrunkTVertex.clear();
  TrunkWVertexwght.clear();
  TrunkTVertexwght.clear();
  lochitlistsize.clear();
  
  lochitlistendsize.clear();
  nhoughlines.clear();
  nhoughlinesend.clear();
  
  
  fRMS_wire.clear();
  fRMS_time.clear();
  
  
  fChisq.clear();
    
  fcorrelation.clear();
  fcovariance.clear();
  
  mcwirevertex.clear();  // wire coordinate of vertex for each plane 
  mctimevertex.clear();  // time coordinate of vertex for each plane
  mcpdg.clear();
  mcenergy.clear();
  mcphi.clear();
  mctheta.clear();
  mcomega.clear();  

  mcx.clear();
  mcy.clear();
  mcz.clear();
  mcplane.clear();
  mcdirection.clear();
  
  
  lineslopetest.clear();
  lineinterctest.clear();

  fOffAxis.clear();
  fOnAxis.clear();
  fOffAxisNorm.clear();
  fOnAxisNorm.clear();
  fNhits.clear();
  fNhitsClust.clear();
  fHitDensity.clear();
  fLength.clear(); 
  
  
   fOffAxisNormHD.clear(); 
   fOnAxisNormHD.clear(); 
   fLengthHD.clear();
    
   fHitDensityOA.clear();
   fOnAxisNormOA.clear(); 
   fLengthOA.clear();
     
   fPrincipalHD.clear();
   fOffAxisNormLength.clear(); 
   slope2DHD.clear();
   fMultiHitWires.clear();
   fMultiHitWiresNorm.clear();
   fOffAxisOnlyOff.clear(); 
   fOffAxisOnlyOffNorm.clear(); 
   
  
  
  fWireVertex.clear();
  fTimeVertex.clear();
  fWireLast.clear();
  fTimeLast.clear();
     
  
  xangle.clear();
  slope_cm.clear();
  slope.clear();
      
  fWireVertex.resize(nClusters); 
  fTimeVertex.resize(nClusters); 
  fWireLast.resize(nClusters); 
  fTimeLast.resize(nClusters); 
    
  
  xangle.resize(nClusters); 
  slope_cm.resize(nClusters); 
  slope.resize(nClusters); 
  
 
  fSingleEvtAngle.resize(nClusters); 
  fSingleEvtAngleVal.resize(nClusters); 
  fShowerWidthProfile2D.resize(nClusters); ;  // vector to show the plane shower Width distribution 
  fShowerChargeProfile2D.resize(nClusters); ;  //vector to show the plane shower Charge distribution
  fShowerPosition2D.resize(nClusters); ;  //vector to store the positions of hit values stored in the previous two

  for(unsigned int ii=0;ii<nClusters;ii++){   
    fSingleEvtAngle[ii].resize(180); 
    fSingleEvtAngleVal[ii].resize(180); 
    fShowerWidthProfile2D[ii].resize(0); ;  // vector to show the plane shower Width distribution 
    fShowerChargeProfile2D[ii].resize(0); ;  //vector to show the plane shower Charge distribution
    fShowerPosition2D[ii].resize(0); ;  //vector to store the positions of hit values stored in the
  }


  // fPitch.resize(fNPlanes); 
 	 
 
  mcwirevertex.resize(nClusters);  // wire coordinate of vertex for each plane 
  mctimevertex.resize(nClusters);  // time coordinate of vertex for each plane
  mcpdg.resize(nClusters);
  mcenergy.resize(nClusters);
  mcphi.resize(nClusters);
  mctheta.resize(nClusters);
  mcomega.resize(nClusters);
    
  mcx.resize(nClusters);
  mcy.resize(nClusters);
  mcz.resize(nClusters);
  mcplane.resize(nClusters);
  mcdirection.resize(nClusters);
  
  
  fOffAxis.resize(fNPlanes);
  fOnAxis.resize(fNPlanes);
  fOffAxisNorm.resize(fNPlanes);
  fOnAxisNorm.resize(fNPlanes);
  fNhits.resize(fNPlanes);
  fNhitsClust.resize(nClusters);
  fHitDensity.resize(fNPlanes);
  fLength.resize(fNPlanes);
  
  
  
  fOffAxisNormHD.resize(fNPlanes); 
  fOnAxisNormHD.resize(fNPlanes); 
  fLengthHD.resize(fNPlanes);
  fPrincipalHD.resize(fNPlanes);
  fOffAxisNormLength.resize(fNPlanes); 
  slope2DHD.resize(fNPlanes);
  fMultiHitWiresNorm.resize(fNPlanes);
  fMultiHitWires.resize(fNPlanes);
  fOffAxisOnlyOff.resize(fNPlanes); 
  fOffAxisOnlyOffNorm.resize(fNPlanes);  
   
   
  fHitDensityOA.resize(fNPlanes);
  fOnAxisNormOA.resize(fNPlanes); 
  fLengthOA.resize(fNPlanes);
     
  
  test_wire_vertex.resize(nClusters);
  test_time_vertex.resize(nClusters);

  
  hough_wire_vertex.resize(nClusters);
  hough_time_vertex.resize(nClusters);
  
  fClusterPlane.resize(nClusters);
  
  fMeanCharge.resize(nClusters);
  fDirection.resize(nClusters);
  fPrincDirection.resize(nClusters);
  fTrunkDirection.resize(nClusters);
  fHoughDirection.resize(nClusters);
  fMultiHitDirection.resize(nClusters);
  fHitDensityDirection.resize(nClusters);
  
  fTotalCharge.resize(nClusters);
  fMaxweight.resize(nClusters);
  fSideChargeStart.resize(nClusters);
  fSideChargeEnd.resize(nClusters);
  TrunkWVertex.resize(nClusters);
  TrunkTVertex.resize(nClusters);
  TrunkWVertexwght.resize(nClusters);
  TrunkTVertexwght.resize(nClusters);
  lochitlistsize.resize(nClusters);
  lochitlistendsize.resize(nClusters);
  nhoughlines.resize(nClusters);
  nhoughlinesend.resize(nClusters);
  
  fHighBin.resize(nClusters);
  fLowBin.resize(nClusters);
  fReHighBin.resize(nClusters);
  fReLowBin.resize(nClusters);
  fVerticalness.resize(nClusters);
  

  fRMS_wire.resize(nClusters);
  fRMS_time.resize(nClusters);
  
  
  fChisq.resize(nClusters);
    
  fcorrelation.resize(nClusters);
  fcovariance.resize(nClusters);
  
  lineslopetest.resize(nClusters);
  lineinterctest.resize(nClusters);

  
  
}
  
  
  
// ***************** //
void cluster::ShowerAngleClusterAna::analyze(const art::Event& evt)
{ 
  fWirePitch = geo->WirePitch(0,1,0);    //wire pitch in cm
  fTimeTick=detp->SamplingRate()/1000.; 

  //define conversion constants
  double fDriftVelocity=larp->DriftVelocity(larp->Efield(),larp->Temperature());
  fWiretoCm=fWirePitch;
  fTimetoCm=fTimeTick*fDriftVelocity;

  mf::LogWarning("ShowerAngleClusterAna") << "In Ana module " ; 
   art::ServiceHandle<cheat::BackTracker> bt;
 

  
  //Find run, subrun and event number:
  fRun = evt.id().run();
  fSubRun = evt.id().subRun();
  fEvent = evt.id().event();

  
  art::Handle< std::vector<recob::Cluster> > clusterListHandle;
  evt.getByLabel(fClusterModuleLabel,clusterListHandle);

  
  art::Handle< std::vector<art::PtrVector < recob::Cluster> > > clusterAssociationHandle;
  matchflag=true;  
  try{
    evt.getByLabel(fClusterModuleLabel,clusterAssociationHandle);
  std::cout << " cluster Assoc Handle size: " << clusterAssociationHandle->size() << std::endl;
      }
  catch(cet::exception e) {
      mf::LogWarning("ShowerAngleClusterAna") << "caught exception \n"
                                   << e;
      matchflag=false;				   
      }
  
  
  art::FindManyP<recob::Hit> fmh(clusterListHandle, evt, fClusterModuleLabel);
  art::PtrVector<recob::Cluster> clusters;


  std::cout << " ++++ Clusters received " << clusterListHandle->size() << " +++++ " << std::endl;
  if(clusterListHandle->size() ==0 )
  {
    std::cout << " no clusters received! exiting " << std::endl;
    return;
  }
  ClearandResizeVectors(clusterListHandle->size());
  // resizing once cluster size is known.
  
  endflag=false;
  std::cout <<  " Getting Vertex? " << std::endl; 
  //GetVertexN(evt);
  
     //sim::ParticleList plist = sim::SimListUtils::GetParticleList(evt,"largeant");
   const sim::ParticleList& plist = bt->ParticleList();
       for ( sim::ParticleList::const_iterator ipar = plist.begin(); ipar!=plist.end(); ++ipar){
	  simb::MCParticle *particle = ipar->second;
	  int parpdg = particle->PdgCode();
	  double parmom = particle->Momentum().P();
	  std::cout << " PDG " << parpdg << " " << parmom << " " << particle->Process() << " TrID:" << particle->TrackId() << " Mother " << particle->Mother() << " " << particle->Vx() <<","<< particle->Vy()<<","<< particle->Vz()<< " --T.size: " << particle->Trajectory().size() << std::endl;
	 // break;
	  }
  
  
  
  for(unsigned int iClust = 0; iClust < clusterListHandle->size(); iClust++){

    art::Ptr<recob::Cluster> cl(clusterListHandle, iClust);
   std::vector< art::Ptr<recob::Hit> > hitlist = fmh.at(iClust);
   
   if (!evt.isRealData())
      GetVertexCluster(hitlist,iClust);
   
   


   
    ///////////////////////////////////

      clusters.push_back(cl);

      std::vector< double > spos=cl->StartPos();
      std::vector< double > sposerr=cl->SigmaStartPos();
      
      std::vector< double > epos=cl->EndPos();
      std::vector< double > eposerr=cl->SigmaEndPos();
      
     // Start positions are determined elsewhere and accepted here
//       if(spos[0]!=0 && spos[1]!=0 && sposerr[0]==0 && sposerr[1]==0 ){
// 	fWireVertex.push_back(spos[0]);
// 	fTimeVertex.push_back(spos[1]);
// 	startflag.push_back(true);
// 	//std::cout << "setting external starting points " << spos[0] << " " << spos[1] <<" " << sposerr[0] <<" "<< sposerr[1] << std::endl;
//       }
// 	  else
// 	startflag.push_back(false); 
	
      ///! Change for accepting DBCluster and cheatcluster, so that it doesn't get fooled.
      startflag.push_back(false); 
      
      
      if(epos[0]!=0 && epos[1]!=0 && eposerr[0]==0 && eposerr[1]==0 ){
	fWireLast.push_back(epos[0]);
	fTimeLast.push_back(epos[1]);
	endflag=true;
	//std::cout << "setting external ending points " << epos[0] << " " << epos[1] << std::endl;
      }
	
    std::cout << " hitlist size: " << hitlist.size() << std::endl;
    if(hitlist.size()<=15 )
	continue;

      
    double lineslope, lineintercept,goodness,wire_start,time_start,wire_end,time_end;
    int nofshowerclusters=0;
   // std::cout << "++++ hitlist size " << hitlist.size() << std::endl;
    
    //////////////////////////////////
   // fCParAlg.Find2DAxisRoughHighCharge(lineslope,lineintercept,goodness,hitlist);
   //  std::cout << "%%%%%%%% lineslope, intercept " << lineslope << " "<< lineintercept << std::endl;
   
    fCParAlg.Find2DAxisRough(lineslope,lineintercept,goodness,hitlist);
    fVerticalness[iClust]=goodness;
     std::cout << "%%%%%%%% lineslope, intercept " << lineslope << " "<< lineintercept << std::endl;
    //if(hitlist_high.size()<=3 )
	//continue;
    fCParAlg.Find2DStartPointsHighCharge( hitlist,wire_start,time_start,wire_end,time_end);
    std::cout << "%%%%%%%% high charge basic start points: (" << wire_start<<","<<time_start<<"), end: ( "<<wire_end << ","<<time_end <<")" <<  std::endl;

    // temporary testing
   // fCParAlg.Find2DStartPointsBasic( hitlist,wire_start,time_start,wire_end,time_end);
   // std::cout << "%%%%%%%% high charge basic start points: (" << wire_start<<","<<time_start<<"), end: ( "<<wire_end << ","<<time_end <<")" <<  std::endl;
    
    fWireVertex[iClust]=wire_start;
    fTimeVertex[iClust]=time_start;
   
    
    
    
    double wstn=0,tstn=0,wendn=0,tendn=0;
    fCParAlg.FindTrunk(hitlist,wstn,tstn,wendn,tendn,lineslope,lineintercept);
    std::cout << "%%%%%%%% trunk start points: (" << wstn<<","<<tstn<<"), end: ( "<<wendn << ","<<tendn <<")" <<  std::endl;
    
    fTrunkDirection[iClust]= (wstn<wendn)  ? 1 : -1 ;     
    std::cout << "%%%%%%%% trunk direction : " << fTrunkDirection[iClust] << std::endl;
    
    double HiBin,LowBin,invHiBin,invLowBin,altWeight;
    fCParAlg.FindDirectionWeights(lineslope,wstn,tstn,wendn,tendn,hitlist,HiBin,LowBin,invHiBin,invLowBin,&altWeight); 
    std::cout << "%%%%%%%% Direction weights:  norm: " << HiBin << " " << LowBin << " Inv: " << invHiBin << " " << invLowBin << std::endl;
    
    if(invHiBin+invLowBin> 1000)
      nofshowerclusters++;
    
    //////Save Shower difference variables:
      // check only for largest cluster maybe?
    unsigned int plane=hitlist[0]->WireID().Plane;
    std::cout << "plane: "  << plane << std::endl;
   
    if(invHiBin+invLowBin > fOffAxis[plane] )
       fOffAxis[plane]=invHiBin+invLowBin;
    
    if(HiBin+LowBin > fOnAxis[plane] )
       fOnAxis[plane]=HiBin+LowBin;
    
   
    
  
     
    
    if((HiBin+LowBin)/hitlist.size() > fOnAxisNorm[plane] )
       fOnAxisNorm[plane]=(HiBin+LowBin)/hitlist.size();
    
    if(hitlist.size() > (unsigned int)fNhits[plane] )
       fNhits[plane]=hitlist.size();
  
    fNhitsClust[iClust]=hitlist.size();
    
          
    /////////////////////////////////
    
    
   
    
    int locDirection=fCParAlg.DecideClusterDirection(hitlist,lineslope,wstn,tstn,wendn,tendn);
     std::cout << "%%%%%%%% direction start points: (" << wstn<<","<<tstn<<"), end: ( "<<wendn << ","<<tendn <<")" << "Direction: " << locDirection << std::endl;
    wire_start=wstn;
    time_start=tstn;
    wire_end=wendn;
    time_end=tendn;
    fDirection[iClust]=locDirection;
    fCParAlg.RefineStartPointsHough(hitlist, wire_start,time_start,wire_end,time_end,locDirection); 
    fHoughDirection[iClust]=locDirection;
   
     std::cout << "%%%%%%%% Hough line refine start points: (" << wire_start<<","<<time_start<<"), end: ( "<<wire_end << ","<<time_end <<")" << "Direction: " << locDirection << std::endl; 
     
    fPrincDirection[iClust]=fCParAlg.FindPrincipalDirection(hitlist,wire_start,time_start,wire_end,time_end,lineslope);
    fMultiHitDirection[iClust]=fCParAlg.FindMultiHitDirection(hitlist,wire_start,time_start,wire_end,time_end,lineslope);
    fHitDensityDirection[iClust]=fCParAlg.FindHitCountDirection(hitlist,wire_start,time_start,wire_end,time_end,lineslope);
    
    
    std::cout << "%%%%%%%% start points after decide direction: (" << wire_start<<","<<time_start<<"), end: ( "<<wire_end << ","<<time_end <<")" << "Direction: " << locDirection << std::endl; 
    
    fWireVertex[iClust]=wire_start;
    fTimeVertex[iClust]=time_start;
    fWireLast[iClust]=wire_end;
    fTimeLast[iClust]=time_end; 
    lineslopetest[iClust]=lineslope; 
    lineinterctest[iClust]=lineintercept;
  
    double length=TMath::Sqrt( (wire_start-wire_end)*(wire_start-wire_end)*fWiretoCm +(time_start-time_end)*(time_start-time_end)*fTimetoCm     );
    std::cout << "%%%%%%%%%%%%%%% length " << length << " fHitDensity: " << hitlist.size()/length << " dw: " << (wire_start-wire_end) << " dt: "<< (time_start-time_end)  << " dw^2+dt^2 " << (wire_start-wire_end)*(wire_start-wire_end)*fWiretoCm +(time_start-time_end)*(time_start-time_end)*fTimetoCm  <<  std::endl;
    
    
     if((invHiBin+invLowBin)/hitlist.size() > fOffAxisNorm[plane] )
    { fOffAxisNorm[plane]=(invHiBin+invLowBin)/hitlist.size();
      fHitDensityOA[plane]=hitlist.size()/length;
      fOnAxisNormOA[plane]=(HiBin+LowBin)/hitlist.size();; 
      fLengthOA[plane]=length;
    
    }
    
  
    if((invHiBin+invLowBin)/length > fOffAxisNormLength[plane] )
    { fOffAxisNormLength[plane]=(invHiBin+invLowBin)/length;  }

    if((altWeight)/length > fOffAxisOnlyOffNorm[plane] )
    { fOffAxisOnlyOffNorm[plane]=(altWeight)/length;  }

    
     if((altWeight) > fOffAxisOnlyOff[plane] )
    { fOffAxisOnlyOff[plane]=(altWeight);  }
    
    
    //    fOffAxisOnlyOff.clear(); 
//    fOffAxisOnlyOffNorm.clear(); 

    
    
  // // need calculation for these
//    fMultiHitWires.clear();
    
    int multihit=fCParAlg.MultiHitWires(hitlist);
    if(multihit > fMultiHitWires[plane])
      fMultiHitWires[plane]=multihit;
   
    if((double)multihit/length > fMultiHitWiresNorm[plane])
      fMultiHitWiresNorm[plane]=(double)multihit/length;
    
    if(hitlist.size()/length > fHitDensity[plane] )
    { fHitDensity[plane]=hitlist.size()/length;
      fOffAxisNormHD[plane]=(invHiBin+invLowBin)/hitlist.size();
      fOnAxisNormHD[plane]=(HiBin+LowBin)/hitlist.size();;
    fLengthHD[plane]=length;
    
    TPrincipal pc(2,"D");
    fCParAlg.GetPrincipal(hitlist,&pc);
    double PrincipalEigenvalue = (*pc.GetEigenValues())[0]; 
    fPrincipalHD[plane]=PrincipalEigenvalue;
    
    double locangle=Get2DAngleForHit( fWireVertex[iClust],fTimeVertex[iClust], hitlist);
    if(locangle>90) locangle-=180;
    if(locangle<-90) locangle+=180;  
     slope2DHD[plane]=locangle;
    
     
     
    }
    if(length > fLength[plane])
      {fLength[plane]=length;   // save longest plane
    
      }
	  
     xangle[iClust]=Get2DAngleForHit( fWireVertex[iClust],fTimeVertex[iClust], hitlist);
    } // End loop on clusters.
  

   

  ////ugly temp fix
  std::vector< double > errors;
  errors.resize(clusterListHandle->size());
  
  for(unsigned int i=0;i<clusterListHandle->size();i++)
  {
   if(fVerticalness[i]<1) 
    errors[i]=0.1;
   else
    errors[i]=10; 
  }
 
  /**Fill the output tree with all information */
  ftree_cluster->Fill();
std::cout << "tree filling " << std::endl;


  
}

////////////////////////////////////////////////////////////////////////////////
// Method to get the 2D angle ogf a Cluster based on its starting wire and time.
////////////////////////////////////////////////////////////////////////////////

double cluster::ShowerAngleClusterAna::Get2DAngleForHit( unsigned int swire,double stime,std::vector < art::Ptr < recob::Hit> > hitlist) {
  
  fh_omega_single->Reset();
  
  unsigned int wire;
  // this should changed on the loop on the cluster of the shower
   for(std::vector < art::Ptr < recob::Hit > >::const_iterator hitIter = hitlist.begin(); hitIter != hitlist.end();  hitIter++){
      art::Ptr<recob::Hit> theHit = (*hitIter);
      double time = theHit->PeakTime();  
      wire=theHit->WireID().Wire; 
      double omx=gser.Get2Dangle((double)wire,(double)swire,time,stime);
      fh_omega_single->Fill(180*omx/TMath::Pi(), theHit->Charge());
     }
    
  double omega = fh_omega_single->GetBinCenter(fh_omega_single->GetMaximumBin());// Mean value of the fit
   
  return omega; // in degrees.
}


////////////////////////////////////////////////////////////////////////////////
// Extract MC Information from Cluster and the particle that is the most responsible for its existence
////////////////////////////////////////////////////////////////////////////////

void cluster::ShowerAngleClusterAna::GetVertexCluster(std::vector < art::Ptr < recob::Hit> > hitlist,int iClust){

  art::ServiceHandle<cheat::BackTracker> bt;
int trkid;
   double purity,maxe;
   
   HitsPurity(hitlist, trkid, purity, maxe);
   if (trkid>0){
        const simb::MCParticle *particle = bt->TrackIDToParticle(trkid);
        const std::vector<sim::IDE> vide = bt->TrackIDToSimIDE(trkid);

     std::cout << " -------- Cluster hit TrackID " << trkid << " PDG: " << particle->PdgCode() << std::endl;	
     mcplane[iClust]=hitlist[0]->WireID().Plane;
     mcpdg[iClust]=particle->PdgCode();
     mcenergy[iClust]=particle->P();  
  
     
    // Find Hit with closest deposition to original x,y,z 
     double x,y,z;
     x=particle->Vx();
     y=particle->Vy();
     z=particle->Vz();
     double minx=x,miny=y,minz=z;
     double mindist=9999999.0;
    std::cout << "---- particle x,y,z" << x << " " << y << " " << z << std::endl; 
    for(size_t h = 0; h < hitlist.size(); ++h){
      art::Ptr<recob::Hit> hit = hitlist[h];
      std::vector<sim::IDE> ides;
      bt->HitToSimIDEs(hit,ides);
      for(size_t e = 0; e < ides.size(); ++e){
	if(ides[e].trackID==-trkid)   // the right particle
	   { double dist=TMath::Sqrt( (x-ides[e].x)*(x-ides[e].x)+(y-ides[e].y)*(y-ides[e].y)+(z-ides[e].z)*(z-ides[e].z));
	     if(dist<mindist) {
	      mindist=dist;
	      minx=ides[e].x;
	      miny=ides[e].y;
	      minz=ides[e].z;
	     }
	   }
	}
  
      }  
     
     std::cout << " min x,y,z " << minx << " "<< miny << " " << minz << std::endl;
     
   // This picks up the original particle's  angles. It remains to be seen, whether this is better, or maybe the angle at actual start of cluster. 
   if (particle->P()){
    double lep_dcosx_truth = particle->Px()/particle->P();
    double lep_dcosy_truth = particle->Py()/particle->P();
    double lep_dcosz_truth = particle->Pz()/particle->P();
    
    mf::LogVerbatim("ShowerAngleClusterAna")  << "-----  cx,cy,cz " << lep_dcosx_truth << " " << lep_dcosy_truth << " " << lep_dcosz_truth << std::endl;
    
    
    mcphi[iClust]=  (lep_dcosx_truth == 0.0 && lep_dcosz_truth == 0.0) ? 0.0 : TMath::ATan2(lep_dcosx_truth,lep_dcosz_truth);
    mctheta[iClust]= (lep_dcosx_truth == 0.0 && lep_dcosy_truth == 0.0 && lep_dcosz_truth == 0.0) ? 0.0 : TMath::Pi()*0.5-TMath::ATan2(std::sqrt(lep_dcosx_truth*lep_dcosx_truth + lep_dcosz_truth*lep_dcosz_truth),lep_dcosy_truth);
    
    
    mcphi[iClust]=180*mcphi[iClust]/TMath::Pi();
    mctheta[iClust]= 180*mctheta[iClust]/TMath::Pi();
    mf::LogVerbatim("ShowerAngleClusterAna")  << "-----  phi, theta " <<  mcphi[iClust] << " " << mctheta[iClust] << std::endl;
    
    } //end if(P() is non-zero )
    
     mcx[iClust]=minx;
     mcy[iClust]=miny;
     mcz[iClust]=minz;

  
  mf::LogVerbatim("ShowerAngleClusterAna") <<"particle->Vx()= "<<minx<<" ,y= "<<miny<<" ,z= "<<minz<<std::endl;
  
  double drifttick=(mcx[iClust]/larp->DriftVelocity(larp->Efield(),larp->Temperature()))*(1./fTimeTick);
  
  const double origin[3] = {0.};
  for(unsigned int iplane=0;iplane<fNPlanes;iplane++)
    {
      double pos[3];
      geo->Plane(iplane).LocalToWorld(origin, pos);
      //planex[p] = pos[0];
      mf::LogVerbatim("ShowerAngleClusterAna")  << "plane X positionp " << iplane << " " << pos[0] << std::endl;

      pos[1]=mcy[iClust];
      pos[2]=mcz[iClust];
      unsigned int wirevertex = geo->NearestWire(pos,iplane);
       

      mcwirevertex[iClust]=wirevertex;  // wire coordinate of vertex for each plane
      mctimevertex[iClust]=drifttick-(pos[0]/larp->DriftVelocity(larp->Efield(),larp->Temperature()))*(1./fTimeTick)+detp->TriggerOffset();  // time coordinate of vertex for each plane

      mf::LogVerbatim("ShowerAngleClusterAna") << "wirevertex= "<< mcwirevertex[iClust]
					    << " timevertex " << mctimevertex[iClust] 
					    << " correction "
					    << (pos[0]/larp->DriftVelocity(larp->Efield(),
									   larp->Temperature()))*(1./fTimeTick) 
					    << " " << pos[0];
 

    }
    
  }	// end if(trkid>0)



   
   
   
}   
   
   
   
 //------------------------------------------------------------------------------
// // old version - picks up the mother particle only 
// void cluster::ShowerAngleClusterAna::GetVertexN(const art::Event& evt){
// 
//   
//   art::Handle< std::vector<simb::MCTruth> > mctruthListHandle;
//   evt.getByLabel("generator",mctruthListHandle);
//   
//   art::PtrVector<simb::MCTruth> mclist;
//   for (unsigned int ii = 0; ii <  mctruthListHandle->size(); ++ii)
//     {
//       art::Ptr<simb::MCTruth> mctparticle(mctruthListHandle,ii);	
//       mclist.push_back(mctparticle);
//     } 
//    
//   mf::LogVerbatim("ShowerAngleClusterAna")  << "%%%%%%% mc size size,  "<<mclist.size() <<    std::endl;
//   
//   
//   art::Ptr<simb::MCTruth> mc(mclist[0]);
//   simb::MCParticle neut(mc->GetParticle(0));
//   
//   mcpdg=neut.PdgCode();
//   mcenergy=neut.P();  
//   
//   if (neut.P()){
//     double lep_dcosx_truth = neut.Px()/neut.P();
//     double lep_dcosy_truth = neut.Py()/neut.P();
//     double lep_dcosz_truth = neut.Pz()/neut.P();
//     
//     mf::LogVerbatim("ShowerAngleClusterAna")  << "-----  cx,cy,cz " << lep_dcosx_truth << " " << lep_dcosy_truth << " " << lep_dcosz_truth << std::endl;
//     
//     
//     mcphi=  (lep_dcosx_truth == 0.0 && lep_dcosz_truth == 0.0) ? 0.0 : TMath::ATan2(lep_dcosx_truth,lep_dcosz_truth);
//     mctheta= (lep_dcosx_truth == 0.0 && lep_dcosy_truth == 0.0 && lep_dcosz_truth == 0.0) ? 0.0 : TMath::Pi()*0.5-TMath::ATan2(std::sqrt(lep_dcosx_truth*lep_dcosx_truth + lep_dcosz_truth*lep_dcosz_truth),lep_dcosy_truth);
//     
//     
//     mcphi=180*mcphi/TMath::Pi();
//     mctheta= 180*mctheta/TMath::Pi();
//     mf::LogVerbatim("ShowerAngleClusterAna")  << "-----  phi, theta " <<  mcphi << " " << mctheta << std::endl;
//     
//   }
//   
//   int npart=0;
//   //  while(&& npart < mc->NParticles() )
//   //     {
//   mf::LogVerbatim("ShowerAngleClusterAna")  << "%%%%%%%####### is PDG: "<< npart <<" " << neut.PdgCode() << std::endl; 
//   //	neut=mc->GetParticle(npart++);
//   
//   //   }       
//   
//   mf::LogVerbatim("ShowerAngleClusterAna")  << "%%%%%%%####### after loop is PDG: "<< npart <<" " << neut.PdgCode() << std::endl; 
//   //if((neut.PdgCode()==11 || neut.PdgCode()==-11 )&& neut.StatusCode()==1){
//   
//   xyz_vertex.resize(3);
//   xyz_vertex[0] =neut.Vx();
//   xyz_vertex[1] =neut.Vy();
//   xyz_vertex[2] =neut.Vz();
//   
//   mf::LogVerbatim("ShowerAngleClusterAna") <<"neut.Vx()= "<<neut.Vx()<<" ,y= "<<neut.Vy()<<" ,z= "<<neut.Vz()<<std::endl;
//   //if(((neut.PdgCode()==11 || neut.PdgCode()==-11 )&& neut.StatusCode()==1))
//   //    break;
//   
//   
//   double drifttick=(xyz_vertex[0]/larp->DriftVelocity(larp->Efield(),larp->Temperature()))*(1./fTimeTick);
//   
//   const double origin[3] = {0.};
//   for(unsigned int iplane=0;iplane<fNPlanes;iplane++)
//     {
//       double pos[3];
//       geo->Plane(iplane).LocalToWorld(origin, pos);
//       //planex[p] = pos[0];
//       mf::LogVerbatim("ShowerAngleClusterAna")  << "plane X positionp " << iplane << " " << pos[0] << std::endl;
// 
//       pos[1]=xyz_vertex[1];
//       pos[2]=xyz_vertex[2];
//       ///\todo: have to change to use cryostat and TPC in NearestWire too
//       unsigned int wirevertex = geo->NearestWire(pos,iplane);
//        
// 
//       mcwirevertex[iplane]=wirevertex;  // wire coordinate of vertex for each plane
//       mctimevertex[iplane]=drifttick-(pos[0]/larp->DriftVelocity(larp->Efield(),larp->Temperature()))*(1./fTimeTick)+detp->TriggerOffset();  // time coordinate of vertex for each plane
// 
//       //fWireVertex[p]=wirevertex;
//       //fTimeVertex[p]=drifttick-(pos[0]/larp->DriftVelocity(larp->Efield(),larp->Temperature()))*(1./fTimeTick)+60;
//       mf::LogVerbatim("ShowerAngleClusterAna") << "wirevertex= "<< wirevertex
// 					    << " timevertex " << mctimevertex[iplane] 
// 					    << " correction "
// 					    << (pos[0]/larp->DriftVelocity(larp->Efield(),
// 									   larp->Temperature()))*(1./fTimeTick) 
// 					    << " " << pos[0];
//  
// 
//     }
// 
// 
// 
//   return (void)0;
// }
  
  


//////////////////////////////////////////////////////
/// Stolen from the T962Anamodule
///
//////////////////////////////////////////////////////


void cluster::ShowerAngleClusterAna::HitsPurity(std::vector< art::Ptr<recob::Hit> > const& hits, int& trackid, double& purity, double& maxe){

  trackid = -1;
  purity = -1;
  art::ServiceHandle<cheat::BackTracker> bt;

  std::map<int,double> trkide;

  for(size_t h = 0; h < hits.size(); ++h){
    art::Ptr<recob::Hit> hit = hits[h];
    std::vector<sim::IDE> ides;
    //bt->HitToSimIDEs(hit,ides);
    std::vector<cheat::TrackIDE> eveIDs = bt->HitToEveID(hit);
    for(size_t e = 0; e < eveIDs.size(); ++e){
      //std::cout<<h<<" "<<e<<" "<<eveIDs[e].trackID<<" "<<eveIDs[e].energy<<" "<<eveIDs[e].energyFrac<<std::endl;
      trkide[eveIDs[e].trackID] += eveIDs[e].energy;
    }
  }

  maxe = -1;
  double tote = 0;

  for (std::map<int,double>::iterator ii = trkide.begin(); ii!=trkide.end(); ++ii){
    tote += ii->second;
    if ((ii->second)>maxe){
      maxe = ii->second;
      trackid = ii->first;
    }
  }

  if (tote>0){
    purity = maxe/tote;
  }
}






namespace cluster {

  DEFINE_ART_MODULE(ShowerAngleClusterAna);

}




