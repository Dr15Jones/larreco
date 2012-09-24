////////////////////////////////////////////////////////////////////////
//
// VertexMatch class
//
// joshua.spitz@yale.edu
//
//  This algorithm is designed to match vertices found with a dedicated vertex finder 
//  (HarrisVertexFinder) and those found with the HoughLineFinder. A weak vertex is a 
//  vertex that has been found using a dedicated vertex finding algorithm only. A strong 
//  vertex is a vertex that has been found using a dedicated vertex finding algorithm and 
//  matched to a crossing of two or more HoughLineFinder lines.
////////////////////////////////////////////////////////////////////////

#include "VertexFinder/VertexMatch.h"
extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
}

// ROOT includes
#include <TCanvas.h>
#include "TDatabasePDG.h"
#include "TSystem.h"

#include <sstream>
#include <fstream>
#include <math.h>
#include <algorithm>

#include "art/Framework/Principal/Event.h" 
#include "fhiclcpp/ParameterSet.h" 
#include "art/Framework/Principal/Handle.h" 
#include "art/Persistency/Common/Ptr.h" 
#include "art/Persistency/Common/PtrVector.h" 
#include "art/Framework/Services/Registry/ServiceHandle.h" 
#include "art/Framework/Services/Optional/TFileService.h" 
#include "art/Framework/Services/Optional/TFileDirectory.h" 
#include "messagefacility/MessageLogger/MessageLogger.h" 

#include "RecoBase/Cluster.h"
#include "RecoBase/EndPoint2D.h"
#include "RecoBase/Hit.h"
#include "Geometry/Geometry.h"
#include "Geometry/CryostatGeo.h"
#include "Geometry/TPCGeo.h"
#include "Geometry/PlaneGeo.h"
#include "Utilities/AssociationUtil.h"

//------------------------------------------------------------------------------
vertex::VertexMatch::VertexMatch(fhicl::ParameterSet const& pset) 
  : fVertexModuleLabel(pset.get< std::string >("VertexModuleLabel"))
  , fHoughModuleLabel (pset.get< std::string >("HoughModuleLabel"))
  , fMaxDistance      (pset.get< double      >("MaxDistance"))
{
  produces< std::vector<recob::EndPoint2D> >();
  produces< art::Assns<recob::EndPoint2D, recob::Hit> >();
}

//------------------------------------------------------------------------------
vertex::VertexMatch::~VertexMatch()
{
}

//------------------------------------------------------------------------------
bool sort_pred(const std::pair<art::Ptr<recob::Hit>,double>& left, 
	       const std::pair<art::Ptr<recob::Hit>,double>& right)
{
  return left.first < right.first;
}

//------------------------------------------------------------------------------
bool sort_pred2(const std::pair<art::Ptr<recob::Hit>,double>& left, 
		const std::pair<art::Ptr<recob::Hit>,double>& right)
{
  return left.second < right.second;
}

//------------------------------------------------------------------------------
void vertex::VertexMatch::produce(art::Event& evt)
{

  art::Handle< std::vector<recob::EndPoint2D> > vertexListHandle;
  evt.getByLabel(fVertexModuleLabel,vertexListHandle);
  
  art::Handle< std::vector<recob::Cluster> > houghListHandle;
  evt.getByLabel(fHoughModuleLabel,houghListHandle);
  
  std::unique_ptr<std::vector<recob::EndPoint2D> > mvertexcol(new std::vector<recob::EndPoint2D>);
  std::unique_ptr< art::Assns<recob::EndPoint2D, recob::Hit> > assn(new art::Assns<recob::EndPoint2D, recob::Hit>);
  
  art::ServiceHandle<geo::Geometry> geom;
  //hits associated with a vertex
  std::vector< art::Ptr<recob::Hit> > vHits;
  art::PtrVector<recob::Hit> vertexhit;
  
  std::vector<double> weakvertexstrength; //strength of weak vertices
  std::vector<double> strongvertexstrength; //strength of strong vertices
  //hits associated with a hough line
  std::vector< art::Ptr<recob::Hit> > hHits;
  art::PtrVector<recob::Hit> houghhit;

  //art::PtrVector< std::pair<recob::Hit,double> > matchedvertex;//vertices associated with a hough line
  
  std::vector< std::pair<art::Ptr<recob::Hit>, double> > matchedvertex;

  art::PtrVector<recob::Hit> strongvertex;
  std::vector< std::pair<art::Ptr<recob::Hit>, double> > strongestvertex; //the strongest strong vertex
  
  art::PtrVector<recob::EndPoint2D> vertIn;

  for(size_t ii = 0; ii < vertexListHandle->size(); ++ii){
    art::Ptr<recob::EndPoint2D> vertex(vertexListHandle, ii);
    vertIn.push_back(vertex);
  }

  art::FindManyP<recob::Hit> fmh(vertexListHandle, evt, fVertexModuleLabel);

  art::PtrVector<recob::Cluster> houghIn;
  for(size_t ii = 0; ii < houghListHandle->size(); ++ii){
    art::Ptr<recob::Cluster> cluster(houghListHandle, ii);
    houghIn.push_back(cluster);
  }

  art::FindManyP<recob::Hit> fmhh(houghListHandle, evt, fHoughModuleLabel);

  unsigned int channel,plane,wire,tpc,cstat;
  double slope,intercept,distance;
  double starttime, endtime;
  int startwire, endwire;
  double strength; //the strength of a strong vertex

  for(unsigned int cs = 0; cs < geom->Ncryostats(); ++cs){
    for(unsigned int t = 0; t < geom->Cryostat(cs).NTPC(); ++t){
      for(unsigned int p = 0; p < geom->Cryostat(cs).TPC(t).Nplanes(); p++) {
	//create the vector of vertex hits 
	art::PtrVector<recob::EndPoint2D>::const_iterator vertexIter = vertIn.begin();
	art::PtrVector<recob::Cluster>::const_iterator houghIter = houghIn.begin();
	for(size_t v = 0; v < vertIn.size(); ++v){
	  // vHits = (*vertexIter)->Hits(p,-1);
	  //      if(vHits.size() > 0){
	  //  	 vertexhit.insert(vertexhit.end(),vHits.begin(),vHits.end());
	  //  	 weakvertexstrength.push_back((*vertexIter)->Strength());
	  //  	 }
	  vHits = fmh.at(v);
	  if(vHits.size() > 0){
	    art::PtrVector<recob::Hit>::const_iterator vertexhitIter = vHits.begin();
	    while (vertexhitIter != vHits.end()){
	      vertexhit.push_back((*vertexhitIter));
	      weakvertexstrength.push_back((*vertexIter)->Strength());
	      vertexhitIter++;
	    }       
	  }
	  
	}// end loop over vertIn 
      
      
	if(vHits.size() == 0)
	  continue;
	
	vHits.clear();
	//loop over vector of hough lines and find the vertex hits that are associated with the hough line(s)
	houghIter = houghIn.begin();  
	size_t ctr = 0;
	while(houghIter!= houghIn.end()){ 
	  houghhit.clear();
	  hHits.clear();
	  plane=-1; 
	  distance=-1.;
	  //create vector of hits associated with hough line
	  hHits = fmhh.at(ctr);
	  
	  if(hHits.size() > 0){
	    
	    art::PtrVector<recob::Hit>::const_iterator hitIter = hHits.begin();
	    while (hitIter!=hHits.end()){
	      houghhit.push_back((*hitIter));
	      hitIter++;
	    }  
	  }
	  if(houghhit.size()){
	    channel=houghhit[0]->Wire()->RawDigit()->Channel();
	    geom->ChannelToWire(channel,cstat,tpc,plane,wire);
	  }
	  if(p==plane && t == tpc && cs == cstat){
	    slope=(*houghIter)->dTdW();
	    intercept=(*houghIter)->StartPos()[1] - slope*(*houghIter)->StartPos()[0];
	    for(unsigned int i=0;i < vertexhit.size(); i++){  
	      
	      distance=-1;
	      channel=vertexhit[i]->Wire()->RawDigit()->Channel();
	      geom->ChannelToWire(channel,cstat,tpc,plane,wire);
	      
	      starttime=(*houghIter)->StartPos()[1];
	      endtime=(*houghIter)->EndPos()[1];
	      startwire=(*houghIter)->StartPos()[0];
	      endwire=(*houghIter)->EndPos()[0];
	      
	      //require the vertices found with HarrisVertexFinder to match up with the endpoints 
	      //(within a window) of a Hough line. A strong vertex matches up with at least two Hough lines. 
	      if(((TMath::Abs((int)(wire-startwire))<fMaxDistance*.0743)
		  ||(TMath::Abs((int)(wire-endwire))<fMaxDistance*.0743)
		  )
		 &&((TMath::Abs(vertexhit[i]->PeakTime()-starttime)<fMaxDistance)
		    ||(TMath::Abs(vertexhit[i]->PeakTime()-endtime)<fMaxDistance)
		    ))          		  distance=(TMath::Abs(vertexhit[i]->PeakTime()-slope*(double)wire-intercept)/(sqrt(pow(.0743*slope,2)+1))); 
	      
	      if(distance<(fMaxDistance+((vertexhit[i]->EndTime()-vertexhit[i]->StartTime())/2.))&&distance>-1)
		matchedvertex.push_back(std::pair<art::Ptr<recob::Hit>,double>(vertexhit[i], weakvertexstrength[i]*sqrt(pow(TMath::Abs(endwire-startwire)*.0743,2)+pow(TMath::Abs(endtime-starttime),2))));
	      //ala strongestvertex.push_back(std::pair<art::PtrVector<recob::Hit>,double>(matchedvertex[i].first,strength));
	    }
	  }
	
	  if(vertexhit.size() == 0 || houghhit.size() == 0){
	    houghIter++;
	    ++ctr;
	    continue;
	  }
	  
	  if(vertexIter!=vertIn.end()) vertexIter++;
	  if(houghIter!=houghIn.end()){
	    houghIter++;
	    ++ctr;
	  }
	}
      
	//sort matchedvertex vector to make it easy to find duplicate entries (strong vertices)
	std::sort(matchedvertex.rbegin(), matchedvertex.rend(),sort_pred);
	
	// the "strength" of a strong vertex is defined as 
	// (HarrisVertexStrength*LengthofHoughLine)_1+(HarrisVertexStrength*LengthofHoughLine)_2+...
	// ...+(HarrisVertexStrength*LengthofHoughLine)_n, where n is the number of vertices 
	// associated with a Hough Line
	for(unsigned int i=0;i < matchedvertex.size(); i++){
	  strength=matchedvertex[i].second;
	  
	  for(unsigned int n=1;n < matchedvertex.size() && i>=n; n++)
	    if(matchedvertex[i].first==matchedvertex[i-n].first)	
	      strength+=matchedvertex[i-n].second;
	  
	  strongvertexstrength.push_back(strength);
	  //make sure there is more than one Hough Line associated with the vertex 
          
	  if(strength>matchedvertex[i].second)
	    strongestvertex.push_back(std::pair<art::Ptr<recob::Hit>,double>(matchedvertex[i].first,strength));
	}
	
      
	//sort the strength of the strong vertices to find the strongest vertex
	std::sort(strongestvertex.rbegin(), strongestvertex.rend(),sort_pred2);
	for(unsigned int i=0;i < matchedvertex.size(); i++){      
	  // I think this is grabbing first item in pair, itself a pointer then grabbing first 
	  // (.begin()) one of those. EC, 18-Oct-2010.
	  channel=(matchedvertex[i].first)->Wire()->RawDigit()->Channel();
	  geom->ChannelToWire(channel,cstat,tpc,plane,wire);
	  
	  // strongvertex, despite name, is a hit vector.
	  strongvertex.push_back(matchedvertex[i].first);
	  
	  //find the strong vertices, those vertices that have been 
	  // associated with more than one hough line  
	  int id = 0;
	  if(i > 0){
	    if(matchedvertex[i].first==matchedvertex[i-1].first){
	      if(strongvertex[0]==(strongestvertex[0].first)&&strongestvertex.size()>0)
		id = 4;//the strongest strong vertex is given a vertex id=4
	      else
		id = 3;//strong vertices are given vertex id=3
	    } 
	  } 
	  else{
	    // weak vertices that have been associated with an endpoint of 
	    // a single Hough line are given vertex id=2	      
	    id = 2;
	  } 
	  
	  // strongvertex is a collection of hits
	  double totalQ = 0.;
	  for(size_t h = 0; h < strongvertex.size(); ++h) totalQ += strongvertex[h]->Charge();

	  recob::EndPoint2D vertex((matchedvertex[i].first)->PeakTime(),
				   wire,
				   strongvertexstrength[i],   
				   id,
				   geom->Cryostat(cs).TPC(tpc).Plane(plane).View(),
				   totalQ);      
	  
	  mvertexcol->push_back(vertex);

	  util::CreateAssn(*this, evt, *(mvertexcol.get()), strongvertex, *(assn.get()));

	  strongvertex.clear();
	  
	}
      
	strongestvertex.clear();
	matchedvertex.clear();
	vertexhit.clear();  
      }// end loop over planes
    }//end loop over tpc
  }// end loop over cryostats

  evt.put(std::move(mvertexcol));
  evt.put(std::move(assn));
}
  


