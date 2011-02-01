////////////////////////////////////////////////////////////////////////
//
// DBSCANfinder.cxx
//
// kinga.partyka@yale.edu
//
//  This algorithm finds clusters of hits, they can be of arbitrary shape.You need to specify 2(3) parameters: 
// epsilon, epsilon2 and MinPoints as explained in the corresponding xml file.In my comments a 'point' reference 
// appears quite often. A 'point' is basically a simple hit which only contains wire and time information. This 
// algorithm is based on DBSCAN(Density Based Spatial Clustering of Applications with Noise): M. Ester, H.-P. Kriegel, 
// J. Sander, and X. Xu, A density-based algorithm for discovering clusters in large spatial databases with noise, 
// Second International Conference on Knowledge Discovery and Data Mining, pp. 226-231, AAAI Press. 1996. 
// ( Some of this code is from "Antonio Gulli's coding playground")  
////////////////////////////////////////////////////////////////////////


//Framework includes:
#include "art/Framework/Core/Event.h"
#include "art/Persistency/Common/Handle.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "art/Framework/Core/TFileDirectory.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "ClusterFinder/DBScanService.h"
#include <cmath>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include "Geometry/geo.h"
#include "RecoBase/recobase.h"

#include "TH1.h"

//----------------------------------------------------------
cluster::DBScanService::DBScanService(fhicl::ParameterSet const& pset, art::ActivityRegistry& reg):
  fEps   (pset.get< double      >("eps")   ),
  fEps2  (pset.get< double      >("epstwo")  ),
  fMinPts(pset.get< int         >("minPts"))
{

}

//----------------------------------------------------------
cluster::DBScanService::~DBScanService()
{
}

//----------------------------------------------------------
void cluster::DBScanService::InitScan(art::PtrVector<recob::Hit>& allhits)
{

  // clear all the data member vectors for the new set of hits
  fps.clear();
  fpointId_to_clusterId.clear();
  fnoise.clear();
  fvisited.clear();
  fsim.clear();
  fsim2.clear();
  fsim3.clear();
  fclusters.clear();

  //------------------------------------------------------------------
  // Determine spacing between wires (different for each detector)
  ///get 2 first wires and find their spacing (wire_dist)
  
  art::ServiceHandle<geo::Geometry> geom;

  const geo::WireGeo& wire = geom->Plane(0).Wire(0);
  const double pos[3] = {0., 0.0, 0.};
  double posWorld0[3] = {0.};
  double posWorld1[3] = {0.};
  wire.LocalToWorld(pos, posWorld0);
  
  //std::cerr << "wire 0, plane 0 test pos in world coordinates is (" << posWorld0[0] << "," << posWorld0[1] << "," << posWorld0[2]<< ")" << std::endl;
  
  const geo::WireGeo& wire1 = geom->Plane(0).Wire(1);
  wire1.LocalToWorld(pos, posWorld1);
  
  //std::cerr << "wire 1, plane 0 test pos in world coordinates is (" << posWorld1[0] << "," << posWorld1[1] << "," << posWorld1[2]<< ")" << std::endl;
  double wire_dist =posWorld0[1]- posWorld1[1];
  //std::cout<<"wire spacing is: "<< wire_dist<<std::endl;
  
  for (unsigned int j = 0; j < allhits.size(); j++){
    int dims=3;//our point is defined by 3 elements:wire#,center of the hit, and the hit width
    std::vector<double> p(dims);
    
    
    p[0] = (allhits[j]->Wire()->RawDigit()->Channel())*wire_dist;
    p[1]=((allhits[j]->StartTime()+allhits[j]->EndTime())/2.)*0.03069;
    p[2]=(allhits[j]->EndTime()-allhits[j]->StartTime())*0.03069;   //width of a hit in cm
    
    fps.push_back(p);
  }

  fpointId_to_clusterId.resize(fps.size(), 0);
  fnoise.resize(fps.size(), false);
  fvisited.resize(fps.size(), false);

  return;
}

//----------------------------------------------------------
double cluster::DBScanService::getSimilarity(const std::vector<double> v1, const std::vector<double> v2){
  
   
  //for Euclidean distance comment everything out except this-->>>
  // return sqrt((v2[1]-v1[1])*(v2[1]-v1[1])+(v2[0]-v1[0])*(v2[0]-v1[0]));
  //------------------------------------------------------------------------
  // return fabs( v2[0]-v1[0]); //for rectangle
  //---------------------------------------------------------------------- 
  //Manhattan distance:
  //return fabs(v1[0]-v2[0])+fabs(v1[1]-v2[1]);
  //---------------------------------------------------------------------
  return (( v2[0]-v1[0])*( v2[0]-v1[0])); //for ellipse

}

//----------------------------------------------------------------
double cluster::DBScanService::getSimilarity2(const std::vector<double> v1, const std::vector<double> v2){

  //-------------------------------------------
  //return fabs( v2[1]-v1[1]);//for rectangle
  //------------------------------------------
  return (( v2[1]-v1[1])*( v2[1]-v1[1]));//for ellipse
  
  
}

//----------------------------------------------------------------
double cluster::DBScanService::getWidthFactor(const std::vector<double> v1, const std::vector<double> v2){
 
  //double k=0.13; //this number was determined by looking at flat muon hits' widths. The average width of these hits in cm is 0.505, so 4*2*(w1^2)=2.04 where w1=w2=0.505, e^2.044= 7.69. In order not to change the distance in time direction of the ellipse we want to make it equal to 1 for these hits. Thus the k factor is k=1/7.69=0.13//for coeff=4

  //std::cout<<"v1[2]= "<<v1[2]<<" v2[2]= "<<v2[2]<<" so the factor is : "<<(( v1[2]*v1[2])+( v2[2]*v2[2]))*k<<std::endl;

  //double k=0.78;
  //..................................................
  double k=0.1;//for 4.5 coeff
  double WFactor=(exp(4.6*(( v1[2]*v1[2])+( v2[2]*v2[2]))))*k;
  //........................................................
  //Let's try something different:
  // double k=1.96;
  //    double WFactor=(( v1[2]*v1[2])+( v2[2]*v2[2]))*k;
  if(WFactor>1){
    if(WFactor<6.25) {return WFactor;}//remember that we are increasing the distance in eps2 as sqrt of this number (i.e sqrt(6.25))
    else {return 6.25;}
   
  }
  else {return 1.0;}
  //.............................................
  
  
  
}

//----------------------------------------------------------------
std::vector<unsigned int> cluster::DBScanService::findNeighbors( unsigned int pid, double threshold,double threshold2)
{
  // std::cout<<"EPSILON IS: "<<threshold<<std::endl;
  //std::cout<<"EPSILON2 IS: "<<threshold2<<std::endl;
  std::vector<unsigned int> ne;
  
  for ( int unsigned j=0; j < fsim.size(); j++)
    {
      
      //  if 	((pid != j ) && ((fsim[pid][j]) < threshold) )//for circle
      //----------------------------------------------------------------------------------------------    
      // if 	((pid != j ) && ((fsim[pid][j]) < threshold) && ((fsim2[pid][j]) < threshold2))//for rectangle
      //----------------------------------------------------------------------------------------------
      if 	((pid != j ) && (((fsim[pid][j])/ (threshold*threshold))+ ((fsim2[pid][j])/ (threshold2*threshold2*(fsim3[pid][j]))))<1) //ellipse
	//*(fsim3[pid][j])))) < 1 ) //for ellipse
	//-------------------------------------------------------------------------------------

	{
	
	  ne.push_back(j);
	  // std::cout<<"Neighbor of pid "<<pid<<" is"<<j<<" their WFactor= "<<fsim3[pid][j]<<std::endl;
	  for(int unsigned i=0; i<ne.size();i++){
	    // std::cout<<ne[i]<<" ";
	  }
	  // std::cout<<std::endl;
	  //std::cout << "sim(" << pid  << "," << j << ")" << fsim(pid, j) << ">" << threshold << std::endl;
	}

      //$$$
      else{
 	if(pid != j ){
	  // std::cout<<"point "<<pid<<" and "<<j<<" NOT neighbors b/c not in ellipse (WFactor= "<<fsim3[pid][j]<<" ), not less than 1 but "<<((fsim[pid][j])/ (threshold*threshold))+ ((fsim2[pid][j])/ (threshold2*threshold2*(fsim3[pid][j])))<<std::endl;
	    
	}
      }
      //$$$
    }

  //$$$
  //  std::cout<<"Neighbors of pid "<<pid<<" : ";
  // 	  for(int unsigned i=0; i<ne.size();i++){
  // 	     std::cout<<ne[i]<<" ";
  // 	  }
  // 	  std::cout<<std::endl;

  //$$$




  return ne;
};

//-----------------------------------------------------------------
void cluster::DBScanService::computeSimilarity()
{
  int size = fps.size();
  fsim.resize(size, std::vector<double>(size));
  //	std::cout<<"running computeSimilarity"<<std::endl;
  for ( int i=0; i < size; i++)
    {
      for ( int j=i+1; j < size; j++)
	{
	  fsim[j] [i] = fsim[i][ j] = getSimilarity(fps[i], fps[j]);
		
		
	  //	std::cout << "similarity for (" << i << ", " << j << ")=" << fsim[i] [j] << " ";
	}
      // std::cout << std::endl;
    }
};

//------------------------------------------------------------------
void cluster::DBScanService::computeSimilarity2()
{
  int size = fps.size();
  fsim2.resize(size, std::vector<double>(size));
  //	std::cout<<"running computeSimilarity"<<std::endl;
  for ( int i=0; i < size; i++)
    {
      for ( int j=i+1; j < size; j++)
	{

	  fsim2[j] [i] = fsim2[i][ j] = getSimilarity2(fps[i], fps[j]);
	  //	fsim[j] [i] = fsim[i][ j] = 0.9;
		
	  //	std::cout << "similarity for (" << i << ", " << j << ")=" << fsim[i] [j] << " ";
	}
      // std::cout << std::endl;
    }
};

//------------------------------------------------------------------
void cluster::DBScanService::computeWidthFactor()
{
  int size = fps.size();
  fsim3.resize(size, std::vector<double>(size));
       
  for ( int i=0; i < size; i++)
    {
      for ( int j=i+1; j < size; j++)
	{

	  fsim3[j] [i] = fsim3[i][ j] = getWidthFactor(fps[i], fps[j]);
		
	  //std::cout<<"width factor is "<<fsim3[i][j]<<" ";
	}
      // std::cout << std::endl;
    }
};

//------------------------------------------------------------------
//single point output
// std::ostream& cluster::operator<<(std::ostream& o,const std::vector<double>& p)
// {
//   o << "{ ";
  
//   for(unsigned int x=0;x<p.size();x++)
//     {
      
//       o<<" "<<p[x];
//       // o<<"SIZE OF POINT IS: "<<p.size()<<" and the point is: "<<p[x];
//     }
//   o << " }, ";
  
//   return o;
// }

// //--------------------------------------------------------------------
// // clusters output
// std::ostream& cluster::operator<<(std::ostream& o, const cluster::DBScanService& cs)
// {
   
//   for(unsigned int i=0;i<cs.fclusters.size();i++)
//     {
      
//       o<<"c("<<i+1<<")=";
      
      
//       for(unsigned int j=0;j<cs.fpointId_to_clusterId.size();j++)
// 	{
// 	  if (cs.fpointId_to_clusterId[j]==(i+1)){
	      
// 	    o<<cs.fps[j];
// 	  }
// 	}//for
//       o << std::endl;
//     }//for
//   return o;
// }

//----------------------------------------------------------------
/////////////////////////////////////////////////////////////////
// This is the algorithm that finds clusters:
void cluster::DBScanService::run_cluster() 
{
  
  
  unsigned int cid = 1;
  // foreach pid
  for ( unsigned int pid = 0; pid < fps.size(); pid++)
    {
      // not already visited
      if (!fvisited[pid]){  
	
	fvisited[pid] = true;
	//me:
	//	std::cout<<"fvisited vector is: ";
	for(unsigned int i=0;i<fvisited.size();i++){
	  // std::cout<<fvisited[i]<<" ";
	}
	//	std::cout<<std::endl;
	// get the neighbors
	std::vector<unsigned int> ne = findNeighbors(pid, fEps,fEps2);
	
	// not enough support -> mark as noise
	if (ne.size() < fMinPts)
	  {
	    // std::cout<<"Not enough neighbors for point: "<<pid<<"(It has: "<<ne.size()<<" ) ! "<<std::endl;
	    fnoise[pid] = true;
	    // std::cout<<"fnoise vector is: ";
	    //for(unsigned int i=0;i<fnoise.size();i++){
	    //  std::cout<<fnoise[i]<<" "; }
	    // std::cout<<std::endl;
	  } 
	    
	else 
	  {
	    
	    // std::cout<<" Yes! point "<<pid<<" has enough neighbors ("<<ne.size()<<")"<<std::endl;
	    
	    // Add p to current cluster
	    
	    std::vector<unsigned int> c;              // a new cluster
	   
	    c.push_back(pid);   	// assign pid to cluster
	    //  std::cout<<"Cluster c now contains the following pids: ";
	    //  for(unsigned int i=0;i<c.size();i++){
	    // 	      // std::cout<<c[i]<<" ";
	    // 	    }
	    // std::cout<<std::endl;
	    fpointId_to_clusterId[pid]=cid;
	    // std::cout<<"fpointId_to_clusterId is now: ";
	    //  for(unsigned int i=0;i<fpointId_to_clusterId.size();i++){
	    // 	      std::cout<<fpointId_to_clusterId[i]<<" ";
	    // 	    }
	    //  std::cout<<std::endl;
	    // go to neighbors
	    for (unsigned int i = 0; i < ne.size(); i++)
	      {
		unsigned int nPid = ne[i];
		
		// not already visited
		if (!fvisited[nPid])
		  {
		    fvisited[nPid] = true;
		    // std::cout<<"fvisited vector is now(2): ";
		    for(unsigned int i=0;i<fvisited.size();i++){
		      // std::cout<<fvisited[i]<<" ";
		    }
		    // std::cout<<std::endl;
		    // go to neighbors
		    std::vector<unsigned int> ne1 = findNeighbors(nPid, fEps, fEps2);
		    //   std::cout<<"found this many: "<<ne1.size()<<" neighbors for pointid "<<nPid<<". THey are: ";
		    //  for(unsigned int i=0;i<ne1.size();i++){
		    // 		      //  std::cout<<ne1[i]<<" ";
		    // 		    }
		    
		    //  std::cout<<std::endl;
		    // enough support
		    if (ne1.size() >= fMinPts)
		      {
		       	
			//	std::cout<<"ok, so let's include pid: "<<nPid<<std::endl;
			// join
			
			for(unsigned int i=0;i<ne1.size();i++)
			  {
			    // join neighbord
			    //ne.push_back(n1); 
			    ne.push_back(ne1[i]); 
			    
			  }
			//	std::cout<<" neighbors for "<<nPid<<" are now: ";
			for(unsigned int i=0;i<ne.size();i++){
			  // std::cout<<ne[i]<<" ";
			}
			//	std::cout << std::endl;
		      }
		  }
		
		// not already assigned to a cluster
		if (!fpointId_to_clusterId[nPid])
		  {
		    //  std::cout<<"pointid: "<<nPid<<" hasn't been added to a cluster, so adding now!"<<std::endl;
		    
		    c.push_back(nPid);
		    fpointId_to_clusterId[nPid]=cid;
		  }
	      }
	    
	    //  std::cout<<"Our cluster #"<<cid<<" has the following pointids: ";
	    // 	    for(unsigned int i=0; i<c.size();i++){
	    // 	      std::cout<<c[i]<<" ";
	      
	    // 	    }
	    // 	     std::cout<<std::endl;
	    fclusters.push_back(c);
	    
	    
	    cid++;
	  }
      } // if (!visited
    } // for
  
  //  std::cout<<"final fnoise vector is: ";
  //    for(unsigned int i=0;i<fnoise.size();i++){
  //      std::cout<<fnoise[i]<<" "; }
  //    std::cout<<std::endl;



  int noise=0;
  //no_hits=fnoise.size();

  std::cout<<"NO OF HITS IS "<<fnoise.size();
   
  
  for(unsigned int y=0;y< fpointId_to_clusterId.size();++y){

    if  (fpointId_to_clusterId[y]==0) noise++;


  }
  
  std::cout<<" , "<<noise<<" is noise"<<std::endl;
   
  std::cout<<"THE CURRENT NOISE LEVEL IS: "<<(double(noise)/double(fnoise.size()))*100<<" %"<<std::endl;
  
}







