/////////////////////////////////////////////////////////////////////
///
/// HoughClusAlg class
///
/// Ben Carls, bcarls@fnal.gov
///
/// This is a heavily modified variant of the original Hough line code (see HoughLineAlg.cxx).
/// It identifies lines in fuzzy clusters (e.g. muon tracks) and splits them off into new clusters. 
///
/// The algorithm is based on the Progressive Probabilistic Hough Transform (PPHT).
/// See the following paper for details:
///
/// J. Matas et al., Robust Detection of Lines Using the Progressive Probabilistic Hough Transform,
/// Computer Vision and Image Understanding, Volume 78, Issue 1, April 2000, Pages 119–137
///
////////////////////////////////////////////////////////////////////////


extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
}

#include <sstream>
#include <fstream>
#include <math.h>
#include <algorithm>
#include <vector>

#include <TF1.h>

#include "art/Framework/Principal/Event.h" 
#include "fhiclcpp/ParameterSet.h" 
#include "art/Framework/Principal/Handle.h" 
#include "art/Persistency/Common/Ptr.h" 
#include "art/Persistency/Common/PtrVector.h" 
#include "art/Framework/Services/Registry/ServiceHandle.h" 
#include "art/Framework/Services/Optional/TFileService.h" 
#include "art/Framework/Services/Optional/TFileDirectory.h" 
#include "messagefacility/MessageLogger/MessageLogger.h" 
 
#include "ClusterFinder/HoughClusAlg.h"
#include "Filters/ChannelFilter.h"
#include "RecoBase/recobase.h"
#include "Geometry/geo.h"
#include "Utilities/LArProperties.h"
#include "Utilities/DetectorProperties.h"
#include "Utilities/AssociationUtil.h"

//------------------------------------------------------------------------------
cluster::HoughClusAlg::HoughClusAlg(fhicl::ParameterSet const& pset) : HoughBaseAlg(pset)
{
  this->reconfigure(pset);
}

//------------------------------------------------------------------------------
cluster::HoughClusAlg::~HoughClusAlg()
{
}

//------------------------------------------------------------------------------
void cluster::HoughClusAlg::reconfigure(fhicl::ParameterSet const& pset)
{ 
  fMaxLines                 = pset.get< int    >("MaxLines"           );
  fMinHits                  = pset.get< int    >("MinHits"            );
  fSaveAccumulator          = pset.get< int    >("SaveAccumulator"    );
  fNumAngleCells            = pset.get< int    >("NumAngleCells"      );
  fMaxDistance              = pset.get< double >("MaxDistance"        );
  fMaxSlope                 = pset.get< double >("MaxSlope"           );
  fRhoZeroOutRange          = pset.get< int    >("RhoZeroOutRange"    );
  fThetaZeroOutRange        = pset.get< int    >("ThetaZeroOutRange"  );
  fRhoResolutionFactor      = pset.get< int    >("RhoResolutionFactor");
  fPerCluster               = pset.get< int    >("HitsPerCluster"     );
  fMissedHits               = pset.get< int    >("MissedHits"         );
  fEndPointCutoff           = pset.get< double >("EndPointCutoff"     );
  fHoughLineMergeAngle      = pset.get< double >("HoughLineMergeAngle");
  fHoughLineMergeCutoff     = pset.get< double >("HoughLineMergeCutoff");
  fParaHoughLineMergeAngle  = pset.get< double >("ParaHoughLineMergeAngle");
  fParaHoughLineMergeCutoff = pset.get< double >("ParaHoughLineMergeCutoff");
  fLineIsolationCut         = pset.get< double >("LineIsolationCut");

  return;
}





//------------------------------------------------------------------------------
size_t cluster::HoughClusAlg::Transform(std::vector<art::Ptr<recob::Hit> >& hits,
    std::vector<unsigned int>     *fpointId_to_clusterId,
    unsigned int clusterId, // The id of the cluster we are examining
    int *nClusters,
    std::vector<unsigned int> corners
    )
{
  
  //art::FindManyP<recob::Hit> fmh(clusIn, evt, label);

  HoughTransform c;
  std::vector< art::Ptr<recob::Hit> > hit;

  art::ServiceHandle<geo::Geometry> geom;
  art::ServiceHandle<util::LArProperties> larprop;
  art::ServiceHandle<util::DetectorProperties> detprop;
  filter::ChannelFilter chanFilt;



  unsigned int channel = hits[0]->Wire()->RawDigit()->Channel();
  unsigned int plane   = 0;
  unsigned int wire    = 0;
  unsigned int wireMax = 0;
  unsigned int tpc     = 0;
  unsigned int cstat   = 0;
  unsigned int p  = 0;
  unsigned int w  = 0;
  unsigned int t  = 0;
  unsigned int cs = 0;
  geom->ChannelToWire(channel, cs, t, p, w);
  std::vector<lineSlope> linesFound;
  linesFound.clear();

  // Store signal and background for linesFound
  std::vector<double> linesFoundSig;
  std::vector<double> linesFoundBkg;


  //mf::LogInfo("HoughClusAlg") << "nClusters is: " << *nClusters;


  //size_t cinctr = 0;
  //geo::View_t    view = geom->Cryostat(cs).TPC(t).Plane(p).View();
  geo::SigType_t sigt = geom->Cryostat(cs).TPC(t).Plane(p).SignalType();
  std::vector<int> skip;  
  hit.clear();
  
  //factor to make x and y scale the same units
  double xyScale  = .001*larprop->DriftVelocity(larprop->Efield(),larprop->Temperature());
  xyScale        *= detprop->SamplingRate()/geom->WirePitch(0,1,p,t,cs);

  mf::LogInfo("HoughClusAlg") << "xyScale: " << xyScale;
  
  int x, y;
  //unsigned int channel, plane, wire, tpc, cstat;
  //there must be a better way to find which plane a cluster comes from
  int dx = geom->Cryostat(cs).TPC(t).Plane(p).Nwires();//number of wires 
  int dy = hits[0]->Wire()->NSignal();//number of time samples. 
  skip.clear();
  skip.resize(hits.size());
  std::vector<int> listofxmax;
  std::vector<int> listofymax;  
  std::vector<int> hitsTemp;        //indecies ofcandidate hits
  std::vector<int> sequenceHolder; //channels of hits in list
  std::vector<int> currentHits;    //working vector of hits 
  std::vector<int> lastHits;       //best list of hits
  std::vector<art::Ptr<recob::Hit> > clusterHits;
  double indcolscaling = 0.;       //a parameter to account for the different 
        			   ////characteristic hit width of induction and collection plane
  double centerofmassx = 0;
  double centerofmassy = 0;
  double denom = 0; 
  double intercept=0.;
  double slope = 0.;
  //this array keeps track of the hits that have already been associated with a line. 
  int xMax = 0;
  int yMax = 0;
  double rho;
  double theta; 
  int accDx(0), accDy(0);





  /// Outline of PPHT, J. Matas et. al. 
  /// ---------------------------------------
  /// 
  ///LOOP over hits, picking a random one
  ///  Enter the point into the accumulator
  ///  IF it is already in the accumulator or part of a line, skip it
  ///  Store it in a vector of points that have been chosen
  ///
  ///  Find max value in accumulator; IF above threshold, create a line
  ///    Subtract points in line from accumulator
  ///    
  ///
  ///END LOOP over hits, picking a random one
  ///

  




  //Init specifies the size of the two-dimensional accumulator 
  //(based on the arguments, number of wires and number of time samples). 
  c.Init(dx,dy,fRhoResolutionFactor,fNumAngleCells);
  // Adds all of the hits to the accumulator
  //std::cout << "Beginning PPHT" << std::endl;
  mf::LogInfo("HoughClusAlg") << "Beginning PPHT";

  c.GetAccumSize(accDy, accDx);

  // count is how many points are left to randomly insert
  unsigned int count = hits.size();
  std::vector<unsigned int> accumPoints;
  accumPoints.resize(hits.size());
  int nAccum = 0;
  //srand((unsigned)time(NULL)); 

  for( ; count > 0; count--){
  

    // The random hit we are examining
    unsigned int randInd = rand() % hits.size();

    //std::cout << "count: " << count << " randInd: " << randInd << std::endl;

    // If the point isn't in the current fuzzy cluster, skip it
    if(fpointId_to_clusterId->at(randInd) != clusterId)
      continue;

    // Skip if it's already in a line
    if(skip[randInd]==1)
      continue;

    // If we have already accumulated the point, skip it
    if(accumPoints[randInd])
      continue;
    accumPoints[randInd]=1;
   


    

   
    // zeroes out the neighborhood of all previous lines  
    for(unsigned int i = 0; i < listofxmax.size(); ++i){
      int yClearStart = listofymax[i] - fRhoZeroOutRange;
      if (yClearStart < 0) yClearStart = 0;
      
      int yClearEnd = listofymax[i] + fRhoZeroOutRange;
      if (yClearEnd >= accDy) yClearEnd = accDy - 1;
      
      int xClearStart = listofxmax[i] - fThetaZeroOutRange;
      if (xClearStart < 0) xClearStart = 0;
      
      int xClearEnd = listofxmax[i] + fThetaZeroOutRange;
      if (xClearEnd >= accDx) xClearEnd = accDx - 1;
      
      for (y = yClearStart; y <= yClearEnd; ++y){
        for (x = xClearStart; x <= xClearEnd; ++x){
          c.SetCell(y,x,0);
        }
      }
    }// end loop over size of listxmax
  
    //find the weightiest cell in the accumulator.
    int maxCell = 0;
    xMax = 0;
    yMax = 0;
    unsigned short channel = hits[randInd]->Wire()->RawDigit()->Channel();
    //geom->ChannelToWire(channel, cstat, tpc, plane, wire);
    geom->ChannelToWire(channel, cstat, tpc, plane, wireMax);
    double peakMax = hits[randInd]->PeakTime();

    // Add the randomly selected point to the accumulator
    maxCell = c.AddPointReturnMax(wireMax, (int)(hits[randInd]->PeakTime()), &yMax, &xMax, fMinHits);
    nAccum++; 

    //std::cout << "cout: " << count << " maxCell: " << maxCell << std::endl;
    //std::cout << "xMax: " << xMax << " yMax: " << yMax << std::endl;


    // The threshold calculation, see http://www.via.cornell.edu/ece547/projects/Hough/webpage/statistics.html
    // accDx is the number of rho bins,m_rowLength
    //TF1 *threshGaus = new TF1("threshGaus","(1/([0]*sqrt(2*TMath::Pi())))*exp(-0.5*pow(((x-[1])/[0]),2))");
    //double sigma = sqrt(((double)nAccum/(double)accDx)*(1-1/(double)accDx));
    //double mean = (double)nAccum/(double)accDx;
    //threshGaus->SetParameter(0,sigma);
    //threshGaus->SetParameter(1,mean);
    //std::cout << "threshGaus mean: " << mean << " sigma: " << sigma << " accDx: " << accDx << std::endl;
    //std::cout << "nAccum: " << nAccum << std::endl;
    //std::cout << "threshGaus integral range: " << mean-2*sigma << " to " << maxCell << std::endl;
    //std::cout << "threshGaus integral: " << threshGaus->Integral(mean-2*sigma,maxCell) << std::endl;
    //std::cout << "threshGaus integral: " << threshGaus->Integral(0,maxCell) << std::endl;


    // The threshold calculation using a Poisson distribution instead
    //double poisProbSum = 0;
    //for(int j = 0; j <= maxCell; j++){
      //double poisProb = TMath::Poisson(j,mean);
      //poisProbSum+=poisProb;
      //std::cout << "Poisson: " << poisProb << std::endl;
    //}
    //std::cout << "Poisson prob sum: " << poisProbSum << std::endl;
    //std::cout << "Probability it is higher: " << 1-poisProbSum << std::endl;

    // Continue if the probability of finding a point, (1-poisProbSum) is the probability of finding a 
    // value of maxCell higher than what it currently is
    //if( (1-poisProbSum) > 1e-13)
      //continue;


    // The threshold calculation using a Poisson distribution instead
    //double binomProbSum = 0;
    //for(int j = 0; j <= maxCell; j++){
      //double binomProb = TMath::BinomialI(1/(double)accDx,nAccum,j);
      //binomProbSum+=binomProb;
      //std::cout << "BinomialI: " << binomProb << std::endl;
    //}
    //std::cout << "BinomialI prob sum: " << binomProbSum << std::endl;
    //std::cout << "Probability it is higher: " << 1-binomProbSum << std::endl;

    // Continue if the probability of finding a point, (1-poisProbSum) is the probability of finding a 
    // value of maxCell higher than what it currently is
    //if( (1-binomProbSum) > 1e-13)
      //continue;





    // Continue if the biggest maximum for the randomly selected point is smaller than fMinHits
    if (maxCell < fMinHits) 
      continue;


    // Find the center of mass of the 3x3 cell system (with maxCell at the center). 
    denom = centerofmassx = centerofmassy = 0;
  
    if(xMax > 0 && yMax > 0 && xMax + 1 < accDx && yMax + 1 < accDy){  
      for(int i = -1; i < 2; ++i){
        for(int j = -1; j < 2; ++j){
          denom += c.GetCell(yMax+i,xMax+j);
          centerofmassx += j*c.GetCell(yMax+i,xMax+j);
          centerofmassy += i*c.GetCell(yMax+i,xMax+j);
        }
      }
      centerofmassx /= denom;
      centerofmassy /= denom;      
    }
    else  centerofmassx = centerofmassy = 0;
  
    //fill the list of cells that have already been found
    listofxmax.push_back(xMax);
    listofymax.push_back(yMax);

    // Find the lines equation
    c.GetEquation(yMax+centerofmassy, xMax+centerofmassx, rho, theta);
    //c.GetEquation(yMax, xMax, rho, theta);
    slope = -1./tan(theta);    
    intercept = (rho/sin(theta));
    //std::cout << std::endl;
    //std::cout << "slope: " << slope << " intercept: " << intercept << std::endl; 
    //mf::LogInfo("HoughClusAlg") << "slope: " << slope << " intercept: " << intercept;
    double distance;
    /// \todo: the collection plane's characteristic hit width's are, 
    /// \todo: on average, about 5 time samples wider than the induction plane's. 
    /// \todo: this is hard-coded for now.
    if(sigt == geo::kInduction)
      indcolscaling = 5.;
    else
      indcolscaling = 0.;
    // What is this?   
    indcolscaling = 0;


    if(!isinf(slope) && !isnan(slope)){
      sequenceHolder.clear();
      hitsTemp.clear();
      //unsigned int fMaxWire = 0;
      double fMaxPeak = 0;
      int iMaxWire = 0;
      //unsigned int fMinWire = 99999999;
      double fMinPeak = 99999999;
      int iMinWire = -1;
      float background = 0;
      for(size_t i = 0; i < hits.size(); ++i){
        if(fpointId_to_clusterId->at(i) != clusterId)
          continue;
        channel = hits[i]->Wire()->RawDigit()->Channel();
        geom->ChannelToWire(channel, cstat, tpc, plane, wire);
        distance = (TMath::Abs(hits[i]->PeakTime()-slope*(double)(wire)-intercept)/(sqrt(pow(xyScale*slope,2)+1)));
      
        if(wire == wireMax){
          //std::cout << "wireMax: " << wireMax << " distance: " << distance << " less than: " << fMaxDistance+((hits[i]->EndTime()-hits[i]->StartTime())/2.)+indcolscaling << std::endl;
        }
       
        if(distance < fMaxDistance+((hits[i]->EndTime()-hits[i]->StartTime())/2.)+indcolscaling && skip[i]!=1){
          hitsTemp.push_back(i);
          sequenceHolder.push_back(channel);
          //if(wire < fMinWire){
            //fMinWire = wire;
            //fMinPeak = hits[i]->PeakTime();
            //iMinWire = i;
          //}
          //if(wire > fMaxWire){
            //fMaxWire = wire;
            //fMaxPeak = hits[i]->PeakTime();
            //iMaxWire = i;
          //}
          if(hits[i]->PeakTime() < fMinPeak){
            //fMinWire = wire;
            fMinPeak = hits[i]->PeakTime();
            iMinWire = i;
          }
          if(hits[i]->PeakTime() > fMaxPeak){
            //fMaxWire = wire;
            fMaxPeak = hits[i]->PeakTime();
            iMaxWire = i;
          }
        }
      }// end loop over hits
   
      //std::cout << "Wire: " << fMinWire << " Peak time: " << hits[iMinWire]->PeakTime() << std::endl; 
      //std::cout << "Wire: " << fMaxWire << " Peak time: " << hits[iMaxWire]->PeakTime() << std::endl; 
   
      if(hitsTemp.size() < 5) continue;
      currentHits.clear();  
      lastHits.clear();
      int j; 
      currentHits.push_back(0);
      for(size_t i = 0; i + 1 < sequenceHolder.size(); ++i){  
        j = 1;
        while((chanFilt.BadChannel(sequenceHolder[i]+j)) == true) j++;
        if(sequenceHolder[i+1]-sequenceHolder[i] <= j + fMissedHits) currentHits.push_back(i+1);
        else if(currentHits.size() > lastHits.size()) {
          lastHits = currentHits;
          currentHits.clear();
        }
        else currentHits.clear();
      } 
      if(currentHits.size() > lastHits.size()) lastHits = currentHits;
      clusterHits.clear();    

      if(lastHits.size() < 5) continue;

      // Find minimum and maximum wires in the Hough line
      //fMaxWire = 0;
      fMaxPeak = 0;
      iMaxWire = 0;
      //fMinWire = 99999999;
      fMinPeak = 99999999;
      iMinWire = -1;
      for(size_t k = 0; k < lastHits.size(); ++k) {
        if(fpointId_to_clusterId->at(hitsTemp[lastHits[k]]) != clusterId)
          continue;
        unsigned short channel = hits[hitsTemp[lastHits[k]]]->Wire()->RawDigit()->Channel();
        geom->ChannelToWire(channel, cstat, tpc, plane, wire);
        //if(wire < fMinWire){
          //fMinWire = wire;
          //fMinPeak = hits[hitsTemp[lastHits[k]]]->PeakTime();
          //iMinWire = k;
        //}
        //if(wire > fMaxWire){
          //fMaxWire = wire;
          //fMaxPeak = hits[hitsTemp[lastHits[k]]]->PeakTime();
          //iMaxWire = k;
        //}
        if(hits[hitsTemp[lastHits[k]]]->PeakTime() < fMinPeak){
          //fMinWire = wire;
          fMinPeak = hits[hitsTemp[lastHits[k]]]->PeakTime();
          iMinWire = hitsTemp[lastHits[k]];
        }
        if(hits[hitsTemp[lastHits[k]]]->PeakTime() > fMaxPeak){
          //fMaxWire = wire;
          fMaxPeak = hits[hitsTemp[lastHits[k]]]->PeakTime();
          iMaxWire = hitsTemp[lastHits[k]];
        }
      }


      // Loop over hits to sum up background around prospective Hough line
      //for(size_t i = 0; i < hits.size(); ++i) {
        ////if(fpointId_to_clusterId->at(i) != clusterId)
          ////continue;
        //channel = hits[i]->Wire()->RawDigit()->Channel();
        //geom->ChannelToWire(channel, cstat, tpc, plane, wire);
        //distance = (TMath::Abs(hits[i]->PeakTime()-slope*(double)(wire)-intercept)/(sqrt(pow(xyScale*slope,2)+1)));

        //// Sum up background hits, use smart distance
        //if(fMaxDistance+((hits[i]->EndTime()-hits[i]->StartTime())/2.)+indcolscaling < distance 
           ////&& distance < 10*fMaxDistance+((hits[i]->EndTime()-hits[i]->StartTime())/2.)+indcolscaling 
           //&& distance < 10*(fMaxDistance+((hits[i]->EndTime()-hits[i]->StartTime())/2.)+indcolscaling) 
           ////&& skip[i]!=1){
          //){
          //double peakTimePerpMin=-(1/slope)*(double)(wire)+hits[iMinWire]->PeakTime()+(1/slope)*(fMinWire);
          //double peakTimePerpMax=-(1/slope)*(double)(wire)+hits[iMaxWire]->PeakTime()+(1/slope)*(fMaxWire);
          //if((-1/slope) > 0 && hits[iMinWire]->PeakTime() < peakTimePerpMin && hits[iMaxWire]->PeakTime() > peakTimePerpMax)
            //background++;
          //if((-1/slope) < 0 && hits[iMinWire]->PeakTime() > peakTimePerpMin && hits[iMaxWire]->PeakTime() < peakTimePerpMax)
            //background++;
        //}
      //}// end loop over hits


      // Evaluate S/sqrt(B)
      //if(background > 0){
        //mf::LogInfo("HoughClusAlg") << "S/sqrt(B): " << lastHits.size()/sqrt(background);
        //mf::LogInfo("HoughClusAlg") << "S/B: " << lastHits.size()/background;
        //mf::LogInfo("HoughClusAlg") << "B/S: " << background/lastHits.size();
      //}



      // Check if the line has at least a vertex-like feature at its end points!
      // The distance is what is used in fuzzy clustering!
      //if(currentHits.size() > lastHits.size()) lastHits = currentHits;
      //clusterHits.clear();    
      double totalQ = 0.;
      //int nCorners = 0;
      //bool minCorner = false;
      //bool maxCorner = false;
      //bool minCorner = true;
      //bool maxCorner = true;
      const double pos[3] = {0., 0.0, 0.};
      double posWorld0[3] = {0.};
      double posWorld1[3] = {0.};
      const geo::WireGeo& wireGeom = geom->Plane(0).Wire(0);
      const geo::WireGeo& wire1Geom = geom->Plane(0).Wire(1);
      wireGeom.LocalToWorld(pos, posWorld0);
      wire1Geom.LocalToWorld(pos, posWorld1);
      double wire_dist = posWorld0[1]- posWorld1[1];
      double tickToDist = larprop->DriftVelocity(larprop->Efield(),larprop->Temperature());
      tickToDist *= 1.e-3 * detprop->SamplingRate(); // 1e-3 is conversion of 1/us to 1/ns
      //double pMax[2];
      //pMax[0] = (hits[iMaxWire]->Wire()->RawDigit()->Channel())*wire_dist;
      //pMax[1] = ((hits[iMaxWire]->StartTime()+hits[iMaxWire]->EndTime())/2.)*tickToDist;
      //double pMin[2];
      //pMin[0] = (hits[iMinWire]->Wire()->RawDigit()->Channel())*wire_dist;
      //pMin[1] = ((hits[iMinWire]->StartTime()+hits[iMinWire]->EndTime())/2.)*tickToDist;
      ////mf::LogInfo("HoughClusAlg") << "fEndPointCutoff: " << fEndPointCutoff;
      //for(size_t k = 0; k < corners.size(); k++){
        //double pCorner[2];
        //pCorner[0] = (hits[corners[k]]->Wire()->RawDigit()->Channel())*wire_dist;
        //pCorner[1] = ((hits[corners[k]]->StartTime()+hits[corners[k]]->EndTime())/2.)*tickToDist;
        //unsigned int cornerWire  = 0;
        //unsigned int cornerChannel = hits[corners[k]]->Wire()->RawDigit()->Channel();
        //geom->ChannelToWire(cornerChannel, cstat, tpc, plane, cornerWire);
        ////mf::LogInfo("HoughClusAlg") << "min dist: " << sqrt(pow(pMin[0]-pCorner[0],2) + pow(pMin[1]-pCorner[1],2));
        ////mf::LogInfo("HoughClusAlg") << "max dist: " << sqrt(pow(pMax[0]-pCorner[0],2) + pow(pMax[1]-pCorner[1],2));
        //if(sqrt( pow(pMin[0]-pCorner[0],2) + pow(pMin[1]-pCorner[1],2)) < fEndPointCutoff){
          //nCorners++;
          //minCorner = true;
        //}
        //if(sqrt( pow(pMax[0]-pCorner[0],2) + pow(pMax[1]-pCorner[1],2)) < fEndPointCutoff){
          //nCorners++;
          //maxCorner = true;
        //}
      //}

      //protection against very steep uncorrelated hits
      //if(TMath::Abs(slope)>fMaxSlope 
         //&& TMath::Abs((*clusterHits.begin())->Wire()->RawDigit()->Channel()-
                      //clusterHits[clusterHits.size()-1]->Wire()->RawDigit()->Channel())>=0
        //)
        //continue;
          
      if(TMath::Abs(slope)>fMaxSlope ){
        //for(size_t i = 0; i < lastHits.size(); ++i) {
          //if(fpointId_to_clusterId->at(hitsTemp[lastHits[i]]) != clusterId)
            //continue;

          //unsigned short channel = hits[hitsTemp[lastHits[i]]]->Wire()->RawDigit()->Channel();
          //geom->ChannelToWire(channel, cstat, tpc, plane, wire);
          //if(accumPoints[hitsTemp[lastHits[i]]]) 
            //c.SubtractPoint(wire, (int)(hits[hitsTemp[lastHits[i]]]->PeakTime()));
          //skip[hitsTemp[lastHits[i]]]=1;
        //}
        continue;
      }





      /// Walk along the Hough line to determine if gaps exist
      /// Start at hit (wire) we are examining first
      ///   Step backward wire by wire until gap gets too large
      ///     Measure distance between point given by equation, and points found in line
      ///   Step foreward wire by wire until gap gets too large
      ///     Measure distance between point given by equation, and points found in line
      ///   If too many gaps found, veto the line

      //std::cout << "Found line!" << std::endl;
      //std::cout << "wireMax: " << wireMax << " peakMax: " << slope*(double)wireMax + intercept << std::endl;
      //std::cout << "Wire: " << fMinWire << " Peak time: " << hits[iMinWire]->PeakTime() << std::endl; 
      //std::cout << "Wire: " << fMaxWire << " Peak time: " << hits[iMaxWire]->PeakTime() << std::endl; 


      
      // Step backward
      //for(int i = (signed int)wireMax; i >= (signed int)fMinWire; --i) {
        //double peakTime = slope*(double)i + intercept;
        //unsigned int lineChannel = geom->PlaneWireToChannel(plane,i,tpc,cstat);
        //std::cout << "i: " << i << " peakTime: " << peakTime << std::endl;
        //double pLineHit[2];
        //pLineHit[0] = lineChannel*wire_dist;
        //pLineHit[1] = peakTime*tickToDist;
        //for(size_t k = 0; k < lastHits.size(); ++k) {
          //if(fpointId_to_clusterId->at(hitsTemp[lastHits[k]]) != clusterId)
            //continue;
              
          //double pRealHit[2];
          //pRealHit[0] = (hits[hitsTemp[lastHits[k]]]->Wire()->RawDigit()->Channel())*wire_dist;
          //pRealHit[1] = ((hits[hitsTemp[lastHits[k]]]->StartTime()+hits[hitsTemp[lastHits[k]]]->EndTime())/2.)*tickToDist;
          //if( sqrt( pow(pLineHit[0]-pRealHit[0],2) + pow(pLineHit[1]-pRealHit[1],2)) < 10){
            //geom->ChannelToWire(hits[hitsTemp[lastHits[k]]]->Wire()->RawDigit()->Channel(), cstat, tpc, plane, wire);
            ////std::cout << "i: " << i << " k: " << k << " wire: " << wire << " distance: " << sqrt( pow(pLineHit[0]-pRealHit[0],2) + pow(pLineHit[1]-pRealHit[1],2)) << std::endl;
          //}
        //}
      //}

      //// Step foreward
      //for(int i = (signed int)wireMax + 1; i <= (signed int)fMaxWire; ++i) {
        //double peakTime = slope*(double)i + intercept;
        //unsigned int lineChannel = geom->PlaneWireToChannel(plane,i,tpc,cstat);
        //std::cout << "i: " << i << " peakTime: " << peakTime << std::endl;
        //double pLineHit[2];
        //pLineHit[0] = lineChannel*wire_dist;
        //pLineHit[1] = peakTime*tickToDist;
        //for(size_t k = 0; k < lastHits.size(); ++k) {
          //if(fpointId_to_clusterId->at(hitsTemp[lastHits[k]]) != clusterId)
            //continue;
              
          //double pRealHit[2];
          //pRealHit[0] = (hits[hitsTemp[lastHits[k]]]->Wire()->RawDigit()->Channel())*wire_dist;
          //pRealHit[1] = ((hits[hitsTemp[lastHits[k]]]->StartTime()+hits[hitsTemp[lastHits[k]]]->EndTime())/2.)*tickToDist;
          //if( sqrt( pow(pLineHit[0]-pRealHit[0],2) + pow(pLineHit[1]-pRealHit[1],2)) < 10){
            //geom->ChannelToWire(hits[hitsTemp[lastHits[k]]]->Wire()->RawDigit()->Channel(), cstat, tpc, plane, wire);
            ////std::cout << "i: " << i << " k: " << k << " wire: " << wire << " distance: " << sqrt( pow(pLineHit[0]-pRealHit[0],2) + pow(pLineHit[1]-pRealHit[1],2)) << std::endl;
          //}
        //}
      //}






      // If the slope is really high, check if it's a fake
      unsigned int numHitsMissing = 0;
      unsigned int numHitsSearched = 0;
      if( fabs(slope) > 5 ) {      
        //std::cout << "peakMax: " << peakMax << " fMinPeak: " << fMinPeak << " fMaxPeak: " << fMaxPeak << std::endl; 
        if( peakMax < fMinPeak || peakMax > fMaxPeak){
          peakMax = 0.5*(fMinPeak+fMaxPeak);
        }

        // Step backward
        for(double i = peakMax; i >= fMinPeak; --i) {
          numHitsSearched++;
          double iWire = (i-intercept)*(1/slope);
          if(iWire < 0) continue;
          //std::cout << "i: " << i << " iWire: " << iWire << " (unsigned int)iWire: " << (unsigned int)iWire << std::endl;
          unsigned int lineChannel = geom->PlaneWireToChannel(plane,(unsigned int)iWire,tpc,cstat);
          //std::cout << "i: " << i << " lineChannel: " << lineChannel << " iWire: " << iWire << " (unsigned int)iWire: " << (unsigned int)iWire << std::endl;
          double pLineHit[2];
          pLineHit[0] = lineChannel*wire_dist;
          pLineHit[1] = i*tickToDist;
          bool foundHit = false;
          for(size_t k = 0; k < lastHits.size(); ++k) {
            if(fpointId_to_clusterId->at(hitsTemp[lastHits[k]]) != clusterId)
              continue;
                
            double pRealHit[2];
            pRealHit[0] = (hits[hitsTemp[lastHits[k]]]->Wire()->RawDigit()->Channel())*wire_dist;
            pRealHit[1] = ((hits[hitsTemp[lastHits[k]]]->StartTime()+hits[hitsTemp[lastHits[k]]]->EndTime())/2.)*tickToDist;
            if( sqrt( pow(pLineHit[0]-pRealHit[0],2) + pow(pLineHit[1]-pRealHit[1],2)) < 0.5){
              geom->ChannelToWire(hits[hitsTemp[lastHits[k]]]->Wire()->RawDigit()->Channel(), cstat, tpc, plane, wire);
              //std::cout << "i: " << i << " k: " << k << " wire: " << wire << " distance: " << sqrt( pow(pLineHit[0]-pRealHit[0],2) + pow(pLineHit[1]-pRealHit[1],2)) << std::endl;
              foundHit = true;
              break;
            }
          }
          if(!foundHit) numHitsMissing++;
        }

        // Step foreward
        for(double i = peakMax + 1; i <= fMaxPeak; ++i) {
          numHitsSearched++;
          double iWire = (i-intercept)*(1/slope);
          if(iWire < 0) continue;
          //std::cout << "i: " << i << " iWire: " << iWire << " (unsigned int)iWire: " << (unsigned int)iWire << std::endl;
          unsigned int lineChannel = geom->PlaneWireToChannel(plane,(unsigned int)iWire,tpc,cstat);
          //std::cout << "i: " << i << " lineChannel: " << lineChannel << " iWire: " << iWire << " (unsigned int)iWire: " << (unsigned int)iWire << std::endl;
          double pLineHit[2];
          pLineHit[0] = lineChannel*wire_dist;
          pLineHit[1] = i*tickToDist;
          bool foundHit = false;
          for(size_t k = 0; k < lastHits.size(); ++k) {
            if(fpointId_to_clusterId->at(hitsTemp[lastHits[k]]) != clusterId)
              continue;
                
            double pRealHit[2];
            pRealHit[0] = (hits[hitsTemp[lastHits[k]]]->Wire()->RawDigit()->Channel())*wire_dist;
            pRealHit[1] = ((hits[hitsTemp[lastHits[k]]]->StartTime()+hits[hitsTemp[lastHits[k]]]->EndTime())/2.)*tickToDist;
            if( sqrt( pow(pLineHit[0]-pRealHit[0],2) + pow(pLineHit[1]-pRealHit[1],2)) < 0.5){
              geom->ChannelToWire(hits[hitsTemp[lastHits[k]]]->Wire()->RawDigit()->Channel(), cstat, tpc, plane, wire);
              //std::cout << "i: " << i << " k: " << k << " wire: " << wire << " distance: " << sqrt( pow(pLineHit[0]-pRealHit[0],2) + pow(pLineHit[1]-pRealHit[1],2)) << std::endl;
              foundHit = true;
              break;
            }
          }
          if(!foundHit) numHitsMissing++;
        }
      }


      //std::cout << "numHitsMissing: " << numHitsMissing << " numHitsSearched: " << numHitsSearched << std::endl;
      //if(numHitsSearched > 0) std::cout << "numHitsMissing/numHitsSearched: " << (double)numHitsMissing/(double)numHitsSearched << std::endl;
      //if(numHitsMissing > 0) continue;
      if(numHitsSearched > 0) 
        if((double)numHitsMissing/(double)numHitsSearched > 0.1)
          continue;



        //for(size_t k = 0; k < lastHits.size(); ++k) {
          //if(fpointId_to_clusterId->at(hitsTemp[lastHits[k]]) != clusterId)
            //continue;
          //geom->ChannelToWire(hits[hitsTemp[lastHits[k]]]->Wire()->RawDigit()->Channel(), cstat, tpc, plane, wire);
          //std::cout << "k: " << k << " wire: " << wire << " peak time: " << hits[hitsTemp[lastHits[k]]]->PeakTime() << std::endl;
        //}













      
      //if(background > 0){
        ////if(hitsTemp.size()/sqrt(background) < 10){
        ////if(hitsTemp.size()/background < 10) {
        ////if(background/lastHits.size() >= 1){
        //if(background/lastHits.size() >= 0.5){
          //for(size_t i = 0; i < lastHits.size(); ++i){
            //unsigned short channel = hits[hitsTemp[lastHits[i]]]->Wire()->RawDigit()->Channel();
            //geom->ChannelToWire(channel, cstat, tpc, plane, wire);
            //c.SubtractPoint(wire, (int)(hits[hitsTemp[lastHits[i]]]->PeakTime()));
            //skip[hitsTemp[lastHits[i]]]=1;
          //}
          //unsigned int wireMin;
          //unsigned int wireMax;
          //geom->ChannelToWire(hits[iMinWire]->Wire()->RawDigit()->Channel(), cstat, tpc, plane, wireMin);
          //geom->ChannelToWire(hits[iMaxWire]->Wire()->RawDigit()->Channel(), cstat, tpc, plane, wireMax);
          //mf::LogInfo("HoughClusAlg") << "Rejected line!";
          //mf::LogInfo("HoughClusAlg") << "Wire: " << wireMin << " Time bin: " << hits[iMinWire]->StartTime(); 
          //mf::LogInfo("HoughClusAlg") << "Wire: " << wireMax << " Time bin: " << hits[iMaxWire]->StartTime(); 
          //continue;
        //}
      //}


      // Add new line to list of lines
      double pCornerMin[2];
      //pCornerMin[0] = (hits[hitsTemp[lastHits.front()]]->Wire()->RawDigit()->Channel())*wire_dist;
      //pCornerMin[1] = ((hits[hitsTemp[lastHits.front()]]->StartTime()+hits[hitsTemp[lastHits.front()]]->EndTime())/2.)*tickToDist;
      pCornerMin[0] = (hits[iMinWire]->Wire()->RawDigit()->Channel())*wire_dist;
      pCornerMin[1] = ((hits[iMinWire]->StartTime()+hits[iMinWire]->EndTime())/2.)*tickToDist;
      double pCornerMax[2];
      //pCornerMax[0] = (hits[hitsTemp[lastHits.back()]]->Wire()->RawDigit()->Channel())*wire_dist;
      //pCornerMax[1] = ((hits[hitsTemp[lastHits.back()]]->StartTime()+hits[hitsTemp[lastHits.back()]]->EndTime())/2.)*tickToDist;
      pCornerMax[0] = (hits[iMaxWire]->Wire()->RawDigit()->Channel())*wire_dist;
      pCornerMax[1] = ((hits[iMaxWire]->StartTime()+hits[iMaxWire]->EndTime())/2.)*tickToDist;
      (*nClusters)++;
      linesFound.push_back(lineSlope(*nClusters-1,slope,intercept,pCornerMin[0],pCornerMin[1],pCornerMax[0],pCornerMax[1]));
      linesFoundSig.push_back(lastHits.size());
      linesFoundBkg.push_back(background);
      //fMaxWire = 0;
      fMaxPeak = 0;
      iMaxWire = 0;
      //fMinWire = 99999999;
      fMinPeak = 99999999;
      iMinWire = -1;
      for(size_t i = 0; i < lastHits.size(); ++i) {
        fpointId_to_clusterId->at(hitsTemp[lastHits[i]]) = *nClusters-1;
        clusterHits.push_back(hits[hitsTemp[lastHits[i]]]);
        totalQ += clusterHits.back()->Charge();
        channel = hits[hitsTemp[lastHits[i]]]->Wire()->RawDigit()->Channel();
        geom->ChannelToWire(channel, cstat, tpc, plane, wire);

        skip[hitsTemp[lastHits[i]]]=1;
        // Subtract points from the accumulator that have already been used
        if(accumPoints[hitsTemp[lastHits[i]]]) 
          c.SubtractPoint(wire, (int)(hits[hitsTemp[lastHits[i]]]->PeakTime()));
        
        if(hits[hitsTemp[lastHits[i]]]->PeakTime() < fMinPeak){
          //fMinWire = wire;
          fMinPeak = hits[hitsTemp[lastHits[i]]]->PeakTime();
          iMinWire = i;
        }
        if(hits[hitsTemp[lastHits[i]]]->PeakTime() > fMaxPeak){
          //fMaxWire = wire;
          fMaxPeak = hits[hitsTemp[lastHits[i]]]->PeakTime();
          iMaxWire = i;
        }
      }

      //std::cout << "wireMax: " << wireMax << " peakMax: " << slope*(double)wireMax + intercept << std::endl;
      //std::cout << "Wire: " << fMinWire << " Peak time: " << hits[iMinWire]->PeakTime() << std::endl; 
      //std::cout << "Wire: " << fMaxWire << " Peak time: " << hits[iMaxWire]->PeakTime() << std::endl; 



      //mf::LogInfo("HoughClusAlg") << "Found line!";
      //mf::LogInfo("HoughClusAlg") << "Wire: " << wireMin << " Time bin: " << hits[iMinWire]->StartTime(); 
      //mf::LogInfo("HoughClusAlg") << "Wire: " << wireMax << " Time bin: " << hits[iMaxWire]->StartTime(); 

       
    }// end if !isnan

    if(linesFound.size()>(unsigned int)fMaxLines)
      break;

  }// end loop over hits




  std::vector<int>     newClusNum; 
  std::vector<double>  newClusDist; 
  
  





  // Do a merge based on distances between line segments instead of endpoints
  //mergeParaHoughLinesBySegment(0,&linesFound,&newClusNum,&newClusDist,xyScale);
  mergeHoughLinesBySegment(0,&linesFound,&newClusNum,&newClusDist,xyScale);

  // Reassign the merged lines
  for(size_t i = 0; i < hits.size(); ++i) {
    if(fpointId_to_clusterId->at(i) == clusterId)
      continue;
    for(unsigned int k = 0; k < linesFound.size(); k++){
      //mf::LogInfo("HoughClusAlg") << fpointId_to_clusterId->at(i)  << linesFound[j].oldClusterNumber;
      if(fpointId_to_clusterId->at(i) == linesFound[k].oldClusterNumber)
        fpointId_to_clusterId->at(i) = linesFound[k].clusterNumber;
    }
  }








  
  // Merge remains of original fuzzy cluster into nearest Hough line, assumes the Hough line is a segment
  //const double pos[3] = {0., 0.0, 0.};
  //double posWorld0[3] = {0.};
  //double posWorld1[3] = {0.};
  //const geo::WireGeo& wireGeom = geom->Plane(0).Wire(0);
  //const geo::WireGeo& wire1Geom = geom->Plane(0).Wire(1);
  //wireGeom.LocalToWorld(pos, posWorld0);
  //wire1Geom.LocalToWorld(pos, posWorld1);
  //double wire_dist = posWorld0[1]- posWorld1[1];
  //double tickToDist = larprop->DriftVelocity(larprop->Efield(),larprop->Temperature());
  //tickToDist *= 1.e-3 * detprop->SamplingRate(); // 1e-3 is conversion of 1/us to 1/ns
  //for(size_t i = 0; i < hits.size(); ++i) {
    //if(fpointId_to_clusterId->at(i) != clusterId)
      //continue;
    //channel = hits[i]->Wire()->RawDigit()->Channel();
    //geom->ChannelToWire(channel, cstat, tpc, plane, wire);
    //double p0 = (hits[i]->Wire()->RawDigit()->Channel())*wire_dist;
    //double p1 = ((hits[i]->StartTime()+hits[i]->EndTime())/2.)*tickToDist;
    //double minDistance = 10000;
    //for(unsigned int k = 0; k < linesFound.size(); k++){
      //double distance = PointSegmentDistance( p0, p1, linesFound[k].pMin0, linesFound[k].pMin1, linesFound[k].pMax0, linesFound[k].pMax1);
      //distance/=sqrt( pow(linesFound[k].pMin0-linesFound[k].pMax0,2)+pow(linesFound[k].pMin1-linesFound[k].pMax1,2));
      ////distance/=sqrt(sqrt( pow(linesFound[k].pMin0-linesFound[k].pMax0,2)+pow(linesFound[k].pMin1-linesFound[k].pMax1,2)));
      //if(distance < minDistance){
        //fpointId_to_clusterId->at(i) = linesFound[k].clusterNumber;
        //minDistance = distance;
      //}
    //}
  //} 



























  // Now do the signal/background check
  //linesnewclus.clear();
  //for(unsigned int i = 0; i < linesfound.size(); i++){
    //if(linesfound[i].clusternumber == clusterid)
      //continue;
    //bool firstline=false;
    //if(linesnewclus.size() == 0){
      //linesnewclus.push_back(linesfound[i].clusternumber);
      //firstline=true;
    //}
    //bool oldline=false;
    //for(unsigned int j = 0; j < linesnewclus.size(); j++){
      //if(linesfound[i].clusternumber == linesnewclus[j]) 
      //oldline = true;    
    //}
    //if(oldline && !firstline) continue;
    //linesnewclus.push_back(linesfound[i].clusternumber);
    //double signaltotal = 0;
    //double backgroundtotal = 0;
    //if(linesfound[i].clusternumber == linesnewclus.back()){
      ////for(unsigned int k = 0; k < newclusnum.size(); k++){
        ////if(linesfound[i].clusternumber == newclusnum[k]){
          ////if(signaltotal > 0) signaltotal+=linesfoundsig[i];
          ////if(backgroundtotal > 0)backgroundtotal+=linesfoundbkg[i];
        ////}
      ////}
      //for(unsigned int k = 0; k < linesfound.size(); k++){
        //if(linesfound[i].clusternumber == linesfound[k].clusternumber){
          ////std::cout << i  << linesfound[i].clusternumber  << linesfoundsig[k]  << linesfoundbkg[k] << std::endl;
          //if(linesfoundsig[k] > 0) signaltotal+=linesfoundsig[k];
          //if(linesfoundbkg[k] > 0) backgroundtotal+=linesfoundbkg[k];
        //}
      //}  
    //}
    ////std::cout << "cluster number: " << linesfound[i].clusternumber << " backgroundtotal/signaltotal: " << backgroundtotal/signaltotal << std::endl;
    //// kill the junk cluster
    //if(backgroundtotal/signaltotal > flineisolationcut){
      //for(unsigned int k = 0; k < linesfound.size(); k++){
        //if(linesnewclus.back() == linesfound[k].clusternumber){
          //linesfound[k].clusternumber = clusterid;
        //}
      //}  
    //}
  //}





  
  
  


  
  










   //// saves a bitmap image of the accumulator (useful for debugging), 
   //// with scaling based on the maximum cell value
   //if(fSaveAccumulator){   
     //unsigned char *outPix = new unsigned char [accDx*accDy];
     ////finds the maximum cell in the accumulator for image scaling
     //int cell, pix = 0, maxCell = 0;
     //for (y = 0; y < accDy; ++y){ 
       //for (x = 0; x < accDx; ++x){
 	//cell = c.GetCell(y,x);
 	//if (cell > maxCell) maxCell = cell;
       //}
     //}
     //for (y = 0; y < accDy; ++y){
       //for (x = 0; x < accDx; ++x){ 
 	////scales the pixel weights based on the maximum cell value     
 	//if(maxCell > 0)
 	  //pix = (int)((1500*c.GetCell(y,x))/maxCell);
 	//outPix[y*accDx + x] = pix;
       //}
     //}
 		
     //SaveBMPFile("houghaccum.bmp", outPix, accDx, accDy);
     //delete [] outPix;
   //}// end if saving accumulator
   
   hit.clear();
   lastHits.clear();
   //if(clusterIter != clusIn.end()){
     //clusterIter++;
     //++cinctr;
   //}
   listofxmax.clear();
   listofymax.clear();









  ////gets the actual two-dimensional size of the accumulator
  //int accDx = 0;
  //int accDy = 0;
  //c.GetAccumSize(accDy, accDx);


  ////
  //// Find the local maxima
  ////

  ////accMaxStore stores the largest value of x for every value of y
  //int accMaxStore[accDy]; 
  //int cell;

  //for (int y = 0; y < accDy; ++y){ 
    //int maxX = 0;
    //for (int x = 0; x < accDx; ++x){
      //cell = c.GetCell(y,x);
      //if (cell > 0) 
        //maxX = x;
    //}
    //accMaxStore[y] = maxX;
  //}

  //for (int y = 0; y < accDy; ++y){ 
    //mf::LogInfo("HoughClusAlg") << accMaxStore[y];
  //}


  // saves a bitmap image of the accumulator (useful for debugging), 
  // with scaling based on the maximum cell value
  if(fSaveAccumulator){   
    unsigned char *outPix = new unsigned char [accDx*accDy];
    //finds the maximum cell in the accumulator for image scaling
    int cell, pix = 0, maxCell = 0;
    for (int y = 0; y < accDy; ++y){ 
      for (int x = 0; x < accDx; ++x){
        cell = c.GetCell(y,x);
        if (cell > maxCell) maxCell = cell;
      }
    }
    for (int y = 0; y < accDy; ++y){
      for (int x = 0; x < accDx; ++x){ 
        //scales the pixel weights based on the maximum cell value     
        if(maxCell > 0)
          pix = (int)((1500*c.GetCell(y,x))/maxCell);
        outPix[y*accDx + x] = pix;
      }
    }
                
    HLSSaveBMPFile("houghaccum.bmp", outPix, accDx, accDy);
    delete [] outPix;
  }// end if saving accumulator


  return 1; 
}





// Merges based on the distance between line segments
void cluster::HoughClusAlg::mergeHoughLinesBySegment(unsigned int clusIndexStart,std::vector<lineSlope> *linesFound,std::vector<int> *newClusNum,std::vector<double> *newClusDist, double xyScale)
{

  // If we have zero or one Hough lines, move on 
  if(linesFound->size() == 0 || linesFound->size() == 1)
    return;

  // If a merge happened, trigger this function again to look at this cluster again!
  bool lineMerged = false;

  
  mf::LogVerbatim("HoughClusAlg") << "Merging with clusIndexStart: " << clusIndexStart;

  // If we reach the last Hough line, move on 
  if(linesFound->size() == clusIndexStart+1)
    return;

  // Min to merge
  std::vector<unsigned int> toMerge; 
  std::vector<double> mergeSlope;
  std::vector<double> mergeTheta;
  std::vector<double> mergeDistSinTheta;

  // Store distances between clusters
  std::vector<double> newClusDistTemp;  
  std::vector<int>    newClusNumTemp; 

  // Check if segments are close enough
  for(unsigned int j = 0; j < linesFound->size(); j++){
    if(linesFound->at(clusIndexStart).clusterNumber != linesFound->at(j).clusterNumber)
      continue;
    for(unsigned int i = 0; i < linesFound->size(); i++){
      if(linesFound->at(clusIndexStart).clusterNumber == linesFound->at(i).clusterNumber)
        continue;
      double segmentDistance = HoughLineDistance(linesFound->at(j).pMin0,linesFound->at(j).pMin1,linesFound->at(j).pMax0, linesFound->at(j).pMax1, 
        linesFound->at(i).pMin0,linesFound->at(i).pMin1,linesFound->at(i).pMax0, linesFound->at(i).pMax1);
      if(segmentDistance<fHoughLineMergeCutoff)
      {
        toMerge.push_back(i);

        //mergeSlope.push_back(linesFound->at(j).clusterSlope);
        mergeSlope.push_back( (linesFound->at(j).pMax1-linesFound->at(j).pMin1)/(linesFound->at(j).pMax0-linesFound->at(j).pMin0) );

        newClusDistTemp.push_back(segmentDistance);
        //std::cout << "Check at min, clusIndexStart: " << clusIndexStart << " mergee: " << i << " merger: " << j << std::endl;
        //std::cout << "Distance: " << segmentDistance << std::endl;
        //std::cout << "p0MinLine1: " <<  linesFound->at(j).pMin0 << " p1MinLine1: " << linesFound->at(j).pMin1 << " p0MaxLine1: " << linesFound->at(j).pMax0 << " p1MaxLine1: " <<  linesFound->at(j).pMax1 << std::endl;
        //std::cout << "p0MinLine2: " <<  linesFound->at(i).pMin0 << " p1MinLine2: " << linesFound->at(i).pMin1 << " p0MaxLine2: " << linesFound->at(i).pMax0 << " p1MaxLine2: " <<  linesFound->at(i).pMax1 << std::endl;
        //mf::LogInfo("HoughClusAlg") << "Check at min, clusIndexStart: " << clusIndexStart << " mergee: " << i << " merger: " << j;
        //mf::LogInfo("HoughClusAlg") << "Distance: " << HoughLineDistance(linesFound->at(j).pMin0,linesFound->at(j).pMin1,linesFound->at(j).pMax0, linesFound->at(j).pMax1, 
        //linesFound->at(i).pMin0,linesFound->at(i).pMin1,linesFound->at(i).pMax0, linesFound->at(i).pMax1);
        //mf::LogInfo("HoughClusAlg") << "p0MinLine1: " <<  linesFound->at(j).pMin0 << " p1MinLine1: " << linesFound->at(j).pMin1 << " p0MaxLine1: " << linesFound->at(j).pMax0 << " p1MaxLine1: " <<  linesFound->at(j).pMax1;
        //mf::LogInfo("HoughClusAlg") << "p0MinLine2: " <<  linesFound->at(i).pMin0 << " p1MinLine2: " << linesFound->at(i).pMin1 << " p0MaxLine2: " << linesFound->at(i).pMax0 << " p1MaxLine2: " <<  linesFound->at(i).pMax1;
      }
    }
  }

  //mergeSlope.resize(toMerge.size());
  mergeTheta.resize(toMerge.size());
  mergeDistSinTheta.resize(toMerge.size());

  // Find slope in cluster being examined to compare to cluster that will be merged into it
  //for(unsigned int j = 0; j < toMerge.size(); j++){
    //for(unsigned int i = 0; i < linesFound->size(); i++){
      //if(linesFound->at(clusIndexStart).clusterNumber != linesFound->at(i).clusterNumber)
        //continue;
      //if(HoughLineDistance(linesFound->at(toMerge[j]).pMin0,linesFound->at(toMerge[j]).pMin1,linesFound->at(toMerge[j]).pMax0, linesFound->at(toMerge[j]).pMax1, 
      //linesFound->at(i).pMin0,linesFound->at(i).pMin1,linesFound->at(i).pMax0, linesFound->at(i).pMax1)<fHoughLineMergeCutoff)
        //mergeSlope[j] = linesFound->at(i).clusterSlope;
    //}
  //}

  // Find the angle between the slopes
  for(unsigned int i = 0; i < mergeTheta.size(); i++){
    //mergeTheta[i] = atan(fabs((linesFound->at(toMerge[i]).clusterSlope*xyScale-mergeSlope[i]*xyScale)/(1 + linesFound->at(toMerge[i]).clusterSlope*xyScale*mergeSlope[i]*xyScale)))*(180/TMath::Pi*());
    //mergeTheta[i] = atan(fabs((linesFound->at(toMerge[i]).clusterSlope-mergeSlope[i])/(1 + linesFound->at(toMerge[i]).clusterSlope*mergeSlope[i])))*(180/TMath::Pi*());

    // Sanity check 
    double toMergeSlope =  (linesFound->at(toMerge[i]).pMax1 - linesFound->at(toMerge[i]).pMin1)/(linesFound->at(toMerge[i]).pMax0 - linesFound->at(toMerge[i]).pMin0);
    mergeTheta[i] = atan(fabs(( toMergeSlope - mergeSlope[i])/(1 + toMergeSlope*mergeSlope[i] )))*(180/TMath::Pi());
    mergeDistSinTheta[i] = newClusDistTemp[i]*sin(mergeTheta[i]); 

    //std::cout << "minTheta: " << minTheta[i] << std::endl; 
    mf::LogInfo("HoughClusAlg") << "minTheta: " << mergeTheta[i]; 
  }

  // Perform the merge
  for(unsigned int i = 0; i < mergeTheta.size(); i++){
    if(mergeTheta[i] < fHoughLineMergeAngle ){
    //if(mergeDistSinTheta[i] < 5 ){
      lineMerged = true;
      linesFound->at(clusIndexStart).merged = true;
      linesFound->at(toMerge[i]).merged = true;

      for(unsigned int j = 0; j < linesFound->size(); j++){
        if((unsigned int)toMerge[i] == j)
          continue;
        if(linesFound->at(j).clusterNumber == linesFound->at(toMerge[i]).clusterNumber)
          linesFound->at(j).clusterNumber = linesFound->at(clusIndexStart).clusterNumber;
      }
      linesFound->at(toMerge[i]).clusterNumber = linesFound->at(clusIndexStart).clusterNumber;


      //linesFound->at(maxMerge[i]).clusterNumber = linesFound->at(clusIndexStart).clusterNumber;
      //newClusDist->push_back(newClusDistTemp[i]);
      //newClusNum->push_back(linesFound->at(clusIndexStart).clusterNumber);
    }  
  }

  if(lineMerged)
    mergeHoughLinesBySegment(clusIndexStart,linesFound,newClusNum,newClusDist,xyScale);
  else
    mergeHoughLinesBySegment(clusIndexStart+1,linesFound,newClusNum,newClusDist,xyScale);
  
  return;

}




// Merges based on the distance between line segments
void cluster::HoughClusAlg::mergeParaHoughLinesBySegment(unsigned int clusIndexStart,std::vector<lineSlope> *linesFound,std::vector<int> *newClusNum,std::vector<double> *newClusDist, double xyScale)
{

  // If we have zero or one Hough lines, move on 
  if(linesFound->size() == 0 || linesFound->size() == 1)
    return;

  // If a merge happened, trigger this function again to look at this cluster again!
  bool lineMerged = false;

  
  mf::LogVerbatim("HoughClusAlg") << "Merging with clusIndexStart: " << clusIndexStart;

  // If we reach the last Hough line, move on 
  if(linesFound->size() == clusIndexStart+1)
    return;

  // Min to merge
  std::vector<unsigned int> toMerge; 
  std::vector<double> mergeSlope;
  std::vector<double> mergeTheta;

  // Store distances between clusters
  std::vector<double> newClusDistTemp;  
  std::vector<int>    newClusNumTemp; 

  // Check if segments are close enough
  for(unsigned int j = 0; j < linesFound->size(); j++){
    if(linesFound->at(clusIndexStart).clusterNumber != linesFound->at(j).clusterNumber)
      continue;
    for(unsigned int i = 0; i < linesFound->size(); i++){
      if(linesFound->at(clusIndexStart).clusterNumber == linesFound->at(i).clusterNumber)
        continue;
      double segmentDistance = HoughLineDistance(linesFound->at(j).pMin0,linesFound->at(j).pMin1,linesFound->at(j).pMax0, linesFound->at(j).pMax1, 
        linesFound->at(i).pMin0,linesFound->at(i).pMin1,linesFound->at(i).pMax0, linesFound->at(i).pMax1);
      if(segmentDistance<fParaHoughLineMergeCutoff
          && segmentDistance>fHoughLineMergeCutoff
          )
      {
        toMerge.push_back(i);

        mergeSlope.push_back( (linesFound->at(j).pMax1-linesFound->at(j).pMin1)/(linesFound->at(j).pMax0-linesFound->at(j).pMin0) );
        

        newClusDistTemp.push_back(sqrt(pow(linesFound->at(j).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(j).pMin1-linesFound->at(i).pMax1,2)));
        //std::cout << "Check at min, clusIndexStart: " << clusIndexStart << " mergee: " << i << " merger: " << j << std::endl;
        //std::cout << "Distance: " << segmentDistance << std::endl;
        //std::cout << "p0MinLine1: " <<  linesFound->at(j).pMin0 << " p1MinLine1: " << linesFound->at(j).pMin1 << " p0MaxLine1: " << linesFound->at(j).pMax0 << " p1MaxLine1: " <<  linesFound->at(j).pMax1 << std::endl;
        //std::cout << "p0MinLine2: " <<  linesFound->at(i).pMin0 << " p1MinLine2: " << linesFound->at(i).pMin1 << " p0MaxLine2: " << linesFound->at(i).pMax0 << " p1MaxLine2: " <<  linesFound->at(i).pMax1 << std::endl;
        //mf::LogInfo("HoughClusAlg") << "Check at min, clusIndexStart: " << clusIndexStart << " mergee: " << i << " merger: " << j;
        //mf::LogInfo("HoughClusAlg") << "Distance: " << HoughLineDistance(linesFound->at(j).pMin0,linesFound->at(j).pMin1,linesFound->at(j).pMax0, linesFound->at(j).pMax1, 
        //linesFound->at(i).pMin0,linesFound->at(i).pMin1,linesFound->at(i).pMax0, linesFound->at(i).pMax1);
        //mf::LogInfo("HoughClusAlg") << "p0MinLine1: " <<  linesFound->at(j).pMin0 << " p1MinLine1: " << linesFound->at(j).pMin1 << " p0MaxLine1: " << linesFound->at(j).pMax0 << " p1MaxLine1: " <<  linesFound->at(j).pMax1;
        //mf::LogInfo("HoughClusAlg") << "p0MinLine2: " <<  linesFound->at(i).pMin0 << " p1MinLine2: " << linesFound->at(i).pMin1 << " p0MaxLine2: " << linesFound->at(i).pMax0 << " p1MaxLine2: " <<  linesFound->at(i).pMax1;
      }
    }
  }

  mergeTheta.resize(toMerge.size());


  // Find the angle between the slopes
  for(unsigned int i = 0; i < mergeTheta.size(); i++){

    // Sanity check 
    double toMergeSlope =  (linesFound->at(toMerge[i]).pMax1 - linesFound->at(toMerge[i]).pMin1)/(linesFound->at(toMerge[i]).pMax0 - linesFound->at(toMerge[i]).pMin0);
    mergeTheta[i] = atan(fabs(( toMergeSlope - mergeSlope[i])/(1 + toMergeSlope*mergeSlope[i] )))*(180/TMath::Pi());
    
    //std::cout << "minTheta: " << minTheta[i] << std::endl; 
    mf::LogInfo("HoughClusAlg") << "minTheta: " << mergeTheta[i]; 
  }

  // Perform the merge
  for(unsigned int i = 0; i < mergeTheta.size(); i++){
    if(mergeTheta[i] < fParaHoughLineMergeAngle ){
      lineMerged = true;
      linesFound->at(clusIndexStart).merged = true;
      linesFound->at(toMerge[i]).merged = true;

      for(unsigned int j = 0; j < linesFound->size(); j++){
        if((unsigned int)toMerge[i] == j)
          continue;
        if(linesFound->at(j).clusterNumber == linesFound->at(toMerge[i]).clusterNumber)
          linesFound->at(j).clusterNumber = linesFound->at(clusIndexStart).clusterNumber;
      }
      linesFound->at(toMerge[i]).clusterNumber = linesFound->at(clusIndexStart).clusterNumber;
    }  
  }

  if(lineMerged)
    mergeHoughLinesBySegment(clusIndexStart,linesFound,newClusNum,newClusDist,xyScale);
  else
    mergeHoughLinesBySegment(clusIndexStart+1,linesFound,newClusNum,newClusDist,xyScale);
  
  return;

}










// The normal merger code that uses the distance between the min and max points
void cluster::HoughClusAlg::mergeHoughLines(unsigned int clusIndexStart,std::vector<lineSlope> *linesFound,std::vector<int> *newClusNum,std::vector<double> *newClusDist, double xyScale)
{

  // If we only have zero or one Hough lines, move on 
  if(linesFound->size() == 0 || linesFound->size() == 1)
    return;

  // If a merge happened, trigger this function again to look at this cluster again!
  bool lineMerged = false;

  
  mf::LogVerbatim("HoughClusAlg") << "Merging with clusIndexStart: " << clusIndexStart;

  // If we reach the last Hough line, move on 
  if(linesFound->size() == clusIndexStart+1)
    return;

  // Min to merge
  std::vector<unsigned int> minMerge; 
  std::vector<double> minSlope;
  std::vector<double> minTheta;
  // Max to merge
  std::vector<unsigned int> maxMerge; 
  std::vector<double> maxSlope;
  std::vector<double> maxTheta;

  // Store distances between clusters
  std::vector<double> newClusDistTemp;  
  std::vector<int>    newClusNumTemp; 

  // Check at its min
  for(unsigned int i = 0; i < linesFound->size(); i++){
    if(linesFound->at(clusIndexStart).clusterNumber == linesFound->at(i).clusterNumber)
      continue;
    for(unsigned int j = 0; j < linesFound->size(); j++){
      if(linesFound->at(clusIndexStart).clusterNumber != linesFound->at(j).clusterNumber)
        continue;
      if(sqrt(pow(linesFound->at(j).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(j).pMin1-linesFound->at(i).pMax1,2))<fHoughLineMergeCutoff
          && linesFound->at(j).pMin0 > linesFound->at(i).pMax0
          ){
        minMerge.push_back(i);
        newClusDistTemp.push_back(sqrt(pow(linesFound->at(j).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(j).pMin1-linesFound->at(i).pMax1,2)));
        //std::cout << "Check at min, clusIndexStart: " << clusIndexStart << " minMerge: " << i << std::endl;
        //std::cout << "Distance: " << sqrt(pow(linesFound->at(j).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(j).pMin1-linesFound->at(i).pMax1,2)) << std::endl;
        mf::LogInfo("HoughClusAlg") << "Check at min, clusIndexStart: " << clusIndexStart << " minMerge: " << i;
        mf::LogInfo("HoughClusAlg") << "Distance: " << sqrt(pow(linesFound->at(j).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(j).pMin1-linesFound->at(i).pMax1,2));
      }
    }
  }

  minSlope.resize(minMerge.size());
  minTheta.resize(minMerge.size());

  // Find slope in cluster being examined to compare to cluster that will be merged into it
  for(unsigned int j = 0; j < minMerge.size(); j++){
    for(unsigned int i = 0; i < linesFound->size(); i++){
      if(linesFound->at(clusIndexStart).clusterNumber != linesFound->at(i).clusterNumber)
        continue;
      if(sqrt(pow(linesFound->at(minMerge[j]).pMax0-linesFound->at(i).pMin0,2)+pow(linesFound->at(minMerge[j]).pMax1-linesFound->at(i).pMin1,2))<fHoughLineMergeCutoff)
        minSlope[j] = linesFound->at(i).clusterSlope;
    }
  }

  // Find the angle between the slopes
  for(unsigned int i = 0; i < minTheta.size(); i++){
    minTheta[i] = atan(fabs((linesFound->at(minMerge[i]).clusterSlope*xyScale-minSlope[i]*xyScale)/(1 + linesFound->at(minMerge[i]).clusterSlope*xyScale*minSlope[i]*xyScale)))*(180/TMath::Pi());
    //std::cout << "minTheta: " << minTheta[i] << std::endl; 
    mf::LogInfo("HoughClusAlg") << "minTheta: " << minTheta[i]; 
  }

  // Perform the merge
  for(unsigned int i = 0; i < minTheta.size(); i++){
    if(minTheta[i] < fHoughLineMergeAngle ){
      
      lineMerged = true;
      linesFound->at(clusIndexStart).merged = true;
      linesFound->at(minMerge[i]).merged = true;

      linesFound->at(minMerge[i]).clusterNumber = linesFound->at(clusIndexStart).clusterNumber;
      newClusDist->push_back(newClusDistTemp[i]);
      newClusNum->push_back(linesFound->at(clusIndexStart).clusterNumber);
    }  
  }


  // Check at its max
  for(unsigned int i = 0; i < linesFound->size(); i++){
    if(linesFound->at(clusIndexStart).clusterNumber == linesFound->at(i).clusterNumber)
      continue;
    for(unsigned int j = 0; j < linesFound->size(); j++){
      if(linesFound->at(clusIndexStart).clusterNumber != linesFound->at(j).clusterNumber)
        continue;
      if(sqrt(pow(linesFound->at(j).pMax0-linesFound->at(i).pMin0,2)+pow(linesFound->at(j).pMax1-linesFound->at(i).pMin1,2))<fHoughLineMergeCutoff
          && linesFound->at(j).pMax0 < linesFound->at(i).pMin0
          ){
        maxMerge.push_back(i);
        //std::cout << "Check at max, clusIndexStart: " << clusIndexStart << " maxMerge: " << i << std::endl;
        //std::cout << "Distance: " << sqrt(pow(linesFound->at(j).pMax0-linesFound->at(i).pMin0,2)+pow(linesFound->at(j).pMax1-linesFound->at(i).pMin1,2)) << std::endl;
        mf::LogInfo("HoughClusAlg") << "Check at max, clusIndexStart: " << clusIndexStart << " maxMerge: " << i;
        mf::LogInfo("HoughClusAlg") << "Distance: " << sqrt(pow(linesFound->at(j).pMax0-linesFound->at(i).pMin0,2)+pow(linesFound->at(j).pMax1-linesFound->at(i).pMin1,2));
        newClusDistTemp.push_back(sqrt(pow(linesFound->at(j).pMax0-linesFound->at(i).pMin0,2)+pow(linesFound->at(j).pMax1-linesFound->at(i).pMin1,2)));
      }
    }
  }

  maxSlope.resize(maxMerge.size());
  maxTheta.resize(maxMerge.size());

  // Find slope in cluster being examined to compare to cluster that will be merged into it
  for(unsigned int j = 0; j < maxMerge.size(); j++){
    for(unsigned int i = 0; i < linesFound->size(); i++){
      if(linesFound->at(clusIndexStart).clusterNumber != linesFound->at(i).clusterNumber)
        continue;
      if(sqrt(pow(linesFound->at(maxMerge[j]).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(maxMerge[j]).pMin1-linesFound->at(i).pMax1,2))<fHoughLineMergeCutoff)
        maxSlope[j] = linesFound->at(i).clusterSlope;
    }
  }

  // Find the angle between the slopes
  for(unsigned int i = 0; i < maxTheta.size(); i++){
    maxTheta[i] = atan(fabs((linesFound->at(maxMerge[i]).clusterSlope*xyScale-maxSlope[i]*xyScale)/(1 + linesFound->at(maxMerge[i]).clusterSlope*xyScale*maxSlope[i]*xyScale)))*(180/TMath::Pi());
    //std::cout << "maxTheta: " << maxTheta[i] << std::endl; 
    //std::cout << "maxTheta: " << maxTheta[i] << std::endl; 
    mf::LogInfo("HoughClusAlg") << "maxTheta: " << maxTheta[i]; 
    mf::LogInfo("HoughClusAlg") << "maxTheta: " << maxTheta[i]; 
  }

  // Perform the merge
  for(unsigned int i = 0; i < maxTheta.size(); i++){
    if(maxTheta[i] < fHoughLineMergeAngle ){
      lineMerged = true;
      linesFound->at(clusIndexStart).merged = true;
      linesFound->at(maxMerge[i]).merged = true;

      linesFound->at(maxMerge[i]).clusterNumber = linesFound->at(clusIndexStart).clusterNumber;
      newClusDist->push_back(newClusDistTemp[i]);
      newClusNum->push_back(linesFound->at(clusIndexStart).clusterNumber);
    }
  }

  if(lineMerged)
    mergeHoughLines(clusIndexStart,linesFound,newClusNum,newClusDist,xyScale);
  else
    mergeHoughLines(clusIndexStart+1,linesFound,newClusNum,newClusDist,xyScale);
  
  return;

}


















 //This merge function specifically looks for parallel lines and merges them

int cluster::HoughClusAlg::mergeParaHoughLines(unsigned int clusIndexStart,std::vector<lineSlope> *linesFound,std::vector<int> *newClusNum,std::vector<double> *newClusDist, double xyScale)
{

  int nParaMerged = 0;

  // If we only have zero or one Hough lines, move on 
  if(linesFound->size() == 0 || linesFound->size() == 1)
    return 0;

  // If a merge happened, trigger this function again to look at this cluster again!
  bool lineMerged = false;

  
  mf::LogVerbatim("HoughClusAlg") << "Merging with clusIndexStart: " << clusIndexStart;

  // If we reach the last Hough line, move on 
  if(linesFound->size() == clusIndexStart+1)
    return 0;

  // Min to merge
  std::vector<unsigned int> minMerge; 
  std::vector<double> minSlope;
  std::vector<double> minTheta;
  // Max to merge
  std::vector<unsigned int> maxMerge; 
  std::vector<double> maxSlope;
  std::vector<double> maxTheta;

  // Store distances between clusters
  std::vector<double> newClusDistTemp;  
  std::vector<int>    newClusNumTemp; 

  // Check at its min
  for(unsigned int i = 0; i < linesFound->size(); i++){
    if(linesFound->at(clusIndexStart).clusterNumber == linesFound->at(i).clusterNumber)
      continue;
    for(unsigned int j = 0; j < linesFound->size(); j++){
      if(linesFound->at(clusIndexStart).clusterNumber != linesFound->at(j).clusterNumber)
        continue;
      if(sqrt(pow(linesFound->at(j).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(j).pMin1-linesFound->at(i).pMax1,2))<20
         && sqrt(pow(linesFound->at(j).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(j).pMin1-linesFound->at(i).pMax1,2))>=2
        ){
        minMerge.push_back(i);
        newClusDistTemp.push_back(sqrt(pow(linesFound->at(j).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(j).pMin1-linesFound->at(i).pMax1,2)));
        //std::cout << "Check at min, clusIndexStart: " << clusIndexStart << " minMerge: " << i << std::endl;
        //std::cout << "Distance: " << sqrt(pow(linesFound->at(j).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(j).pMin1-linesFound->at(i).pMax1,2)) << std::endl;
        mf::LogInfo("HoughClusAlg") << "Check at min, clusIndexStart: " << clusIndexStart << " minMerge: " << i;
        mf::LogInfo("HoughClusAlg") << "Distance: " << sqrt(pow(linesFound->at(j).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(j).pMin1-linesFound->at(i).pMax1,2));
      }
    }
  }

  minSlope.resize(minMerge.size());
  minTheta.resize(minMerge.size());

  // Find slope in cluster being examined to compare to cluster that will be merged into it
  for(unsigned int j = 0; j < minMerge.size(); j++){
    for(unsigned int i = 0; i < linesFound->size(); i++){
      if(linesFound->at(clusIndexStart).clusterNumber != linesFound->at(i).clusterNumber)
        continue;
      if(sqrt(pow(linesFound->at(minMerge[j]).pMax0-linesFound->at(i).pMin0,2)+pow(linesFound->at(minMerge[j]).pMax1-linesFound->at(i).pMin1,2))<20)
        minSlope[j] = linesFound->at(i).clusterSlope;
    }
  }

  // Find the angle between the slopes
  for(unsigned int i = 0; i < minTheta.size(); i++){
    minTheta[i] = atan(fabs((linesFound->at(minMerge[i]).clusterSlope*xyScale-minSlope[i]*xyScale)/(1 + linesFound->at(minMerge[i]).clusterSlope*xyScale*minSlope[i]*xyScale)))*(180/TMath::Pi());
    //std::cout << "minTheta: " << minTheta[i] << std::endl; 
    mf::LogInfo("HoughClusAlg") << "minTheta: " << minTheta[i]; 
  }

  // Perform the merge
  for(unsigned int i = 0; i < minTheta.size(); i++){
    if(minTheta[i] < 5 ){
    //if(minTheta[i] < 10 ){
      linesFound->at(minMerge[i]).clusterNumber = linesFound->at(clusIndexStart).clusterNumber;
      lineMerged = true;
      linesFound->at(clusIndexStart).merged = true;
      linesFound->at(minMerge[i]).merged = true;
      newClusDist->push_back(newClusDistTemp[i]);
      newClusNum->push_back(linesFound->at(clusIndexStart).clusterNumber);
      nParaMerged++;
    }  
  }


  // Check at its max
  for(unsigned int i = 0; i < linesFound->size(); i++){
    if(linesFound->at(clusIndexStart).clusterNumber == linesFound->at(i).clusterNumber)
      continue;
    for(unsigned int j = 0; j < linesFound->size(); j++){
      if(linesFound->at(clusIndexStart).clusterNumber != linesFound->at(j).clusterNumber)
        continue;
      if(sqrt(pow(linesFound->at(j).pMax0-linesFound->at(i).pMin0,2)+pow(linesFound->at(j).pMax1-linesFound->at(i).pMin1,2))<20
         && sqrt(pow(linesFound->at(j).pMax0-linesFound->at(i).pMin0,2)+pow(linesFound->at(j).pMax1-linesFound->at(i).pMin1,2))>=2
        ){
        maxMerge.push_back(i);
        //std::cout << "Check at max, clusIndexStart: " << clusIndexStart << " maxMerge: " << i << std::endl;
        //std::cout << "Distance: " << sqrt(pow(linesFound->at(j).pMax0-linesFound->at(i).pMin0,2)+pow(linesFound->at(j).pMax1-linesFound->at(i).pMin1,2)) << std::endl;
        mf::LogInfo("HoughClusAlg") << "Check at max, clusIndexStart: " << clusIndexStart << " maxMerge: " << i;
        mf::LogInfo("HoughClusAlg") << "Distance: " << sqrt(pow(linesFound->at(j).pMax0-linesFound->at(i).pMin0,2)+pow(linesFound->at(j).pMax1-linesFound->at(i).pMin1,2));
        newClusDistTemp.push_back(sqrt(pow(linesFound->at(j).pMax0-linesFound->at(i).pMin0,2)+pow(linesFound->at(j).pMax1-linesFound->at(i).pMin1,2)));
      }
    }
  }

  maxSlope.resize(maxMerge.size());
  maxTheta.resize(maxMerge.size());

  // Find slope in cluster being examined to compare to cluster that will be merged into it
  for(unsigned int j = 0; j < maxMerge.size(); j++){
    for(unsigned int i = 0; i < linesFound->size(); i++){
      if(linesFound->at(clusIndexStart).clusterNumber != linesFound->at(i).clusterNumber)
        continue;
      if(sqrt(pow(linesFound->at(maxMerge[j]).pMin0-linesFound->at(i).pMax0,2)+pow(linesFound->at(maxMerge[j]).pMin1-linesFound->at(i).pMax1,2))<20)
        maxSlope[j] = linesFound->at(i).clusterSlope;
    }
  }

  // Find the angle between the slopes
  for(unsigned int i = 0; i < maxTheta.size(); i++){
    maxTheta[i] = atan(fabs((linesFound->at(maxMerge[i]).clusterSlope*xyScale-maxSlope[i]*xyScale)/(1 + linesFound->at(maxMerge[i]).clusterSlope*xyScale*maxSlope[i]*xyScale)))*(180/TMath::Pi());
    //std::cout << "maxTheta: " << maxTheta[i] << std::endl; 
    mf::LogInfo("HoughClusAlg") << "maxTheta: " << maxTheta[i]; 
  }

  // Perform the merge
  for(unsigned int i = 0; i < maxTheta.size(); i++){
    if(maxTheta[i] <  5 ){
    //if(maxTheta[i] < 10 ){
      linesFound->at(maxMerge[i]).clusterNumber = linesFound->at(clusIndexStart).clusterNumber;
      lineMerged = true;
      linesFound->at(clusIndexStart).merged = true;
      linesFound->at(maxMerge[i]).merged = true;
      newClusDist->push_back(newClusDistTemp[i]);
      newClusNum->push_back(linesFound->at(clusIndexStart).clusterNumber);
      nParaMerged++;
    }
  }

  if(lineMerged)
    nParaMerged += mergeParaHoughLines(clusIndexStart,linesFound,newClusNum,newClusDist,xyScale);
  else
    nParaMerged += mergeParaHoughLines(clusIndexStart+1,linesFound,newClusNum,newClusDist,xyScale);
  
  return nParaMerged;

}






//------------------------------------------------------------------------------
double cluster::HoughClusAlg::HoughLineDistance(double p0MinLine1, double p1MinLine1, double p0MaxLine1, double p1MaxLine1, double p0MinLine2, double p1MinLine2, double p0MaxLine2, double p1MaxLine2)
{
  //distance between two segments in the plane:
  //  one segment is (x11, y11) to (x12, y12) or (p0MinLine1, p1MinLine1) to (p0MaxLine1, p1MaxLine1)
  //  the other is   (x21, y21) to (x22, y22) or (p0MinLine2, p1MinLine2) to (p0MaxLine2, p1MaxLine2)
  double x11 = p0MinLine1; 
  double y11 = p1MinLine1; 
  double x12 = p0MaxLine1; 
  double y12 = p1MaxLine1; 
  double x21 = p0MinLine2; 
  double y21 = p1MinLine2; 
  double x22 = p0MaxLine2; 
  double y22 = p1MaxLine2; 

  if(HoughLineIntersect(x11, y11, x12, y12, x21, y21, x22, y22)) return 0;
  // try each of the 4 vertices w/the other segment
  std::vector<double> distances;
  distances.push_back(PointSegmentDistance(x11, y11, x21, y21, x22, y22));
  distances.push_back(PointSegmentDistance(x12, y12, x21, y21, x22, y22));
  distances.push_back(PointSegmentDistance(x21, y21, x11, y11, x12, y12));
  distances.push_back(PointSegmentDistance(x22, y22, x11, y11, x12, y12));

  double minDistance = 999999; 
  for(unsigned int j = 0; j < distances.size(); j++){
    if (distances[j] < minDistance)
      minDistance = distances[j];
  }
  
  return minDistance;

}




//This portion of the code has a bug in it, of that I am certain
bool cluster::HoughClusAlg::HoughLineIntersect(double x11,double  y11,double  x12,double  y12,double  x21,double  y21,double  x22,double  y22)
{
  //whether two segments in the plane intersect:
  //one segment is (x11, y11) to (x12, y12)
  //the other is   (x21, y21) to (x22, y22)
  
  double dx1 = x12 - x11; // x2-x1
  double dy1 = y12 - y11; // y2-y1
  double dx2 = x22 - x21; // x4-x3
  double dy2 = y22 - y21; // y4-y3
  //double delta = dx2*dy1 - dy2*dx1; // (x4-x3)(y2-y1) - (y4-y3)(x2-x1)
  double delta = dy2*dx1 - dx2*dy1; // (y4-y3)(x2-x1) - (x4-x3)(y2-y1) 
  if (delta == 0) return false;  // parallel segments

  double t = (dx2*(y11 - y21) + dy2*(x21 - x11)) / delta; // ua
  double s = (dx1*(y11 - y21) + dy1*(x21 - x11)) / delta; // ub
  
  return (0 <= s && s <= 1 && 0 <= t && t <= 1);

}



double cluster::HoughClusAlg::PointSegmentDistance(double px,double  py,double  x1,double  y1,double  x2,double  y2)
{
  double dx = x2 - x1;
  double dy = y2 - y1;
  if ( dx == 0 && dy == 0 )  // the segment's just a point
    return sqrt( pow(px - x1,2) + pow(py - y1,2));

  // Calculate the t that minimizes the distance.
  double t = ((px - x1)*dx + (py - y1)*dy) / (dx*dx + dy*dy);

  // See if this represents one of the segment's
  // end points or a point in the middle.
  if(t < 0){
    dx = px - x1;
    dy = py - y1;
  }
  else if(t > 1) {
    dx = px - x2;
    dy = py - y2;
  }
  else if(0 <= t && t <= 1) {
    double near_x = x1 + t * dx;
    double near_y = y1 + t * dy;
    dx = px - near_x;
    dy = py - near_y;
  }

  return sqrt(dx*dx + dy*dy);

}









