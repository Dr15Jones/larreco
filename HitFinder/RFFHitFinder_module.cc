#ifndef RFFHITFINDER_H
#define RFFHITFINDER_H

////////////////////////////////////////////////////////////////////////
//
// RealFrickinFast HitFinder class
//
// wketchum@fnal.edu
//
//  This algorithm is designed to find hits on wires after deconvolution.
//
// 
////////////////////////////////////////////////////////////////////////

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
}
#include <stdint.h>

// Framework includes
#include "art/Framework/Core/ModuleMacros.h" 
#include "art/Framework/Principal/Event.h"   
#include "art/Framework/Services/Optional/TFileService.h"
#include "art/Framework/Core/EDProducer.h" 


// LArSoft Includes
#include "Geometry/Geometry.h"
#include "Geometry/CryostatGeo.h"
#include "Geometry/TPCGeo.h"
#include "Geometry/PlaneGeo.h"
#include "RecoBase/Hit.h"

// ROOT Includes 
#include "TH1D.h"
#include "TDecompSVD.h"
#include "TMath.h"
#include "TF1.h"
#include <string>
#include <vector>
#include "TTree.h"

const float SQRT_TWO_PI = std::sqrt(2*3.14159);
const float GAUS_5SIGMA_TENTH[51] = { 1.000000, 
				      0.995012, 0.980199, 0.955997, 0.923116, 0.882497, 0.835270, 0.782705, 0.726149, 0.666977, 0.606531, 
				      0.546074, 0.486752, 0.429557, 0.375311, 0.324652, 0.278037, 0.235746, 0.197899, 0.164474, 0.135335, 
				      0.110251, 0.088922, 0.071005, 0.056135, 0.043937, 0.034047, 0.026121, 0.019841, 0.014921, 0.011109, 
				      0.008189, 0.005976, 0.004318, 0.003089, 0.002187, 0.001534, 0.001065, 0.000732, 0.000498, 0.000335, 
				      0.000224, 0.000148, 0.000097, 0.000063, 0.000040, 0.000025, 0.000016, 0.000010, 0.000006, 0.000004 };

namespace hit{
  class RFFHitFinder : public art::EDProducer {
    
  public:
    
    explicit RFFHitFinder(fhicl::ParameterSet const& pset); 
    virtual ~RFFHitFinder();

    std::vector<float> GaussianElimination(std::vector< std::vector<float> >, std::vector<float>);

    void produce(art::Event& evt); 
    void beginJob(); 
    void endJob(); 
    void reconfigure(fhicl::ParameterSet const& p);                

  private:
    std::string     fCalDataModuleLabel;
    double          fMinSigInd;     ///<Induction signal height threshold 
    double          fMinSigCol;     ///<Collection signal height threshold 
    double          fIndWidth;      ///<Initial width for induction fit
    double          fColWidth;      ///<Initial width for collection fit
    double          fIndMinWidth;   ///<Minimum induction hit width
    double          fColMinWidth;   ///<Minimum collection hit width
    int             fMaxMultiHit;   ///<maximum hits for multi fit 
    int             fAreaMethod;    ///<Type of area calculation  
    std::vector<double> fAreaNorms; ///<factors for converting area to same units as peak height 
    double	    fChi2NDF;       ///maximum Chisquared / NDF allowed for a hit to be saved
    
    	double	WireNumber[100000];
	double 	TotalSignal[100000];
	double 	StartIime;
	double 	StartTimeError;
	double 	EndTime;
	double 	EndTimeError;
	int	NumOfHits;
	double	MeanPosition;
	double 	MeanPosError;
	double	Amp;
	double	AmpError;
	double	Charge;
	double	ChargeError;
	double	FitGoodnes;
		
  protected: 
    
  
  }; // class RFFHitFinder
  

//-------------------------------------------------
//-------------------------------------------------
RFFHitFinder::RFFHitFinder(fhicl::ParameterSet const& pset)
{
    this->reconfigure(pset);
    produces< std::vector<recob::Hit> >();
}


//-------------------------------------------------
//-------------------------------------------------
  RFFHitFinder::~RFFHitFinder()
{
  // Clean up dynamic memory and other resources here.

}
  
//-------------------------------------------------
//-------------------------------------------------
void RFFHitFinder::reconfigure(fhicl::ParameterSet const& p)
{
  // Implementation of optional member function here.
  fCalDataModuleLabel = p.get< std::string  >("CalDataModuleLabel");
  fMinSigInd          = p.get< double       >("MinSigInd");
  fMinSigCol          = p.get< double       >("MinSigCol"); 
  fIndWidth           = p.get< double       >("IndWidth");  
  fColWidth           = p.get< double       >("ColWidth");
  fIndMinWidth        = p.get< double       >("IndMinWidth");
  fColMinWidth        = p.get< double       >("ColMinWidth"); 	  	
  fMaxMultiHit        = p.get< int          >("MaxMultiHit");
  fAreaMethod         = p.get< int          >("AreaMethod");
  fAreaNorms          = p.get< std::vector< double > >("AreaNorms");
  fChi2NDF	      = p.get< double       >("Chi2NDF");
  
}  

//-------------------------------------------------
//-------------------------------------------------
void RFFHitFinder::beginJob()
{



}

//-------------------------------------------------
//-------------------------------------------------
void RFFHitFinder::endJob()
{

}

// Function for solving a system of linear equations.
// Input goes like this: scaled_distances are a vector of vectors that contain the distances of each peak mean from a given point
// Each internal vector should describe the distance for each peak at a given peak's mean (with that mean corresponding to the element of peak height)
std::vector<float> RFFHitFinder::GaussianElimination(std::vector< std::vector<float> > scaled_distances, std::vector<float> peak_heights){

  std::vector<float> solutions;
  const size_t N_PEAKS = scaled_distances.size();

  if(peak_heights.size() != N_PEAKS ) {
    std::cout << "ERROR!!!! cannot do Guassian elimination of non-square matrix" << std::endl;
    return solutions;
  }

  float matrix[N_PEAKS][N_PEAKS+1]; //N_PEAKS rows, N_PEAKS+1 columns for augmented matrix

  for(int i=0; i<N_PEAKS; i++){
    std::vector<float> this_row = scaled_distances.at(i);
    
    if(this_row.size()!=N_PEAKS){
      std::cout << "ERROR!!!! cannot do Guassian elimination of non-square matrix" << std::endl;
      return solutions;
    }
    
    for(int j=0; j<N_PEAKS; j++){
      float gaus_val = 0;
      if(this_row.at(j) < 4) {
	int this_bin = std::floor( this_row.at(j)*10 + 0.5);
	gaus_val = GAUS_5SIGMA_TENTH[this_bin];
      }      
      matrix[i][j] = gaus_val;
    }
    matrix[i][N_PEAKS] = peak_heights.at(i);

  }//end filling the augmented matrix

  for(int i=0; i<N_PEAKS; i++){
    
    float diag_val = matrix[i][i];
    
    //if(diag_val<0.01){
    //std::cout << "ERROR!!! The diagonal should never be this small?    " << matrix[i][i] << std:: endl;
    //}

    for(int j=i+1; j<N_PEAKS; j++){
      float scale_val = matrix[j][i] / diag_val;
      for(int k=i; k<N_PEAKS+1; k++){
	matrix[j][k] -= matrix[i][k]*scale_val;
      }
    }

  }//end doing the Gaussian elimination

  //solution to the last one is this element
  //std::cout << "OK, we are going to get " << N_PEAKS << " solutions." << std::endl;
  solutions.resize(N_PEAKS);
  //solutions.insert(solutions.begin()+(N_PEAKS-1),matrix[N_PEAKS-1][N_PEAKS]/matrix[N_PEAKS-1][N_PEAKS-1]);

  for(int i=N_PEAKS-1; i>=0; i--){
    
    //std::cout << "getting solution " << i+1 << std::endl;

    float this_solution = matrix[i][N_PEAKS];
    
    for(int j=i+1; j<N_PEAKS; j++)
      this_solution -= matrix[i][j]*solutions.at(j);
    
    this_solution /= matrix[i][i];

    solutions.at(i) = this_solution;
    //std::cout << "\t\tSolution here was " << this_solution << std::endl;
  }

  //std::cout << "We are going to exit with a vector that has " << solutions.size() << " solutions." << std::endl;

  return solutions;
}


//  This algorithm uses the fact that deconvolved signals are very smooth 
//  and looks for hits as areas between local minima that have signal above 
//  threshold.
//-------------------------------------------------
void RFFHitFinder::produce(art::Event& evt)
{
  TH1::AddDirectory(kFALSE);


  // ---------------------------
  // --- Seed Fit Functions  --- (This function used to "seed" the mean peak position)
  // ---------------------------
  TF1 *hit	= new TF1("hit","gaus",0,3200);

  std::unique_ptr<std::vector<recob::Hit> > hcol(new std::vector<recob::Hit>);
    
  // ##########################################
  // ### Reading in the Wire List object(s) ###
  // ##########################################
  art::Handle< std::vector<recob::Wire> > wireVecHandle;
  evt.getByLabel(fCalDataModuleLabel,wireVecHandle);
  art::ServiceHandle<geo::Geometry> geom;
    
  // #########################################################
  // ### List of useful variables used throughout the code ###
  // #########################################################  
  double threshold              = 0.;               // minimum signal size for id'ing a hit
  double fitWidth               = 0.;               //hit fit width initial value
  double minWidth               = 0.;               //minimum hit width
  uint32_t channel              = 0;                // channel number
  geo::SigType_t sigType;                           // Signal Type (Collection or Induction)
  std::vector<int> startTimes;             	    // stores time of 1st local minimum
  std::vector<int> maxTimes;    	   	    // stores time of local maximum    
  std::vector<int> endTimes;    	     	    // stores time of 2nd local minimum
  int time             = 0;                         // current time bin
  int minTimeHolder    = 0;                         // current start time

  std::string eqn        = "gaus(0)";      // string for equation for gaus fit
  //std::string seed        = "gaus(0)";      // string for equation seed gaus fit
  
  
  bool maxFound        = false;            // Flag for whether a peak > threshold has been found
  std::stringstream numConv;

  //##############################
  //### Looping over the wires ###
  //############################## 
  
  for(size_t wireIter = 0; wireIter < wireVecHandle->size(); wireIter++){
    art::Ptr<recob::Wire> wire(wireVecHandle, wireIter);
	
	
    // --- Setting Channel Number and Wire Number as well as signal type ---
    channel = wire->RawDigit()->Channel();
    sigType = geom->SignalType(channel);
      
    // -----------------------------------------------------------
    // -- Clearing variables at the start of looping over wires --
    // -----------------------------------------------------------
    startTimes.clear();
    maxTimes.clear();
    endTimes.clear();
    std::vector<float> signal(wire->Signal());
    std::vector<float>::iterator timeIter;  	    // iterator for time bins
    time          = 0;
    minTimeHolder = 0;
    maxFound      = false;
    // -- Setting the appropriate signal widths and thresholds --
    // --    for either the collection or induction plane      --
    if(sigType == geo::kInduction){
      threshold     = fMinSigInd;
      fitWidth      = fIndWidth;
      minWidth      = fIndMinWidth;
    }//<-- End if Induction Plane
    else if(sigType == geo::kCollection){
      threshold = fMinSigCol;
      fitWidth  = fColWidth;
      minWidth  = fColMinWidth;
    }//<-- End if Collection Plane
      
      
    // ##################################
    // ### Looping over Signal Vector ###
    // ##################################
    for(timeIter = signal.begin();timeIter+2<signal.end();timeIter++){
      // ##########################################################
      // ###                LOOK FOR A MINIMUM                  ###
      // ### Testing if the point timeIter+1 is at a minimum by ###
      // ###  checking if timeIter and timeIter+1 and if it is  ###
      // ###         then we add this to the endTimes           ###
      // ##########################################################
      if(*timeIter > *(timeIter+1) && *(timeIter+1) < *(timeIter+2)){
	//--- Note: We only keep the a minimum if we've already ---
	//---          found a point above threshold            ---
	if(maxFound){
	  endTimes.push_back(time+1);
	  maxFound = false;
	  //keep these in case new hit starts right away
	  minTimeHolder = time+2; 
	}
	else minTimeHolder = time+1; 
	  
      }//<---End Checking if this is a minimum
	
	
      // ########################################################	
      // ### Testing if the point timeIter+1 is a maximum and ###
      // ###  if it and is above threshold then we add it to  ###
      // ###                  the startTime                   ###
      // ########################################################
      //if not a minimum, test if we are at a local maximum 
      //if so, and the max value is above threshold, add it and proceed.
      else if(*timeIter < *(timeIter+1) && *(timeIter+1) > *(timeIter+2) && *(timeIter+1) > threshold){ 
	maxFound = true;
	maxTimes.push_back(time+1);
	startTimes.push_back(minTimeHolder);          
      }
	
      time++;
	
    }//end loop over signal vec 
      
    // ###########################################################    
    // ### If there was no minimum found before the end, but a ### 
    // ###  maximum was found then add an end point to the end ###
    // ###########################################################
    while( maxTimes.size() > endTimes.size() ){ 
      endTimes.push_back(signal.size()-1);
	
    }//<---End maxTimes.size > endTimes.size
	
    // ####################################################
    // ### If no startTime hit was found skip this wire ###
    // ####################################################
    if( startTimes.size() == 0 ){
      continue;
    }  
		     
    /////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////

    // ##########################################################
    // ### PERFORM THE FITTING, ADDING HITS TO THE HIT VECTOR ###
    // ##########################################################

    //All code below does the fitting, adding of hits
    //to the hit vector and when all wires are complete 
    //saving them 
	
    double totSig(0); 						//stores the total hit signal
    double startT(0); 						//stores the start time
    double endT(0);  						//stores the end time
    int numHits(0);  						//number of consecutive hits being fitted
    int size(0);     						//size of data vector for fit
    int hitIndex(0);						//index of current hit in sequence
    double amplitude(0);             			        //fit parameters
    double minPeakHeight(0);  					//lowest peak height in multi-hit fit
		
	
    StartIime = 0; 							// stores the start time of the hit
    StartTimeError = 0;						// stores the error assoc. with the start time of the hit
    EndTime = 0;							// stores the end time of the hit
    EndTimeError = 0;						// stores the error assoc. with the end time of the hit
    MeanPosition = 0;						// stores the peak time position of the hit
    MeanPosError = 0;						// stores the error assoc. with thte peak time of the hit
    Charge = 0;      						// stores the total charge assoc. with the hit
    ChargeError = 0;              					// stores the error on the charge
    Amp = 0;							// stores the amplitude of the hit
    AmpError = 0;							// stores the error assoc. with the amplitude
    NumOfHits = 0;   						// stores the multiplicity of the hit
    FitGoodnes = 0;							// stores the Chi2/NDF of the hit
	
	
     
    //stores gaussian paramters first index is the hit number
    //the second refers to height, position, and width respectively
    std::vector<double>  hitSig;	
	
    // ########################################
    // ### Looping over the hitIndex Vector ###
    // ########################################
    while( hitIndex < (signed)startTimes.size() ) {
      // --- Zeroing Start Times and End Times ---
      startT = 0;
      endT   = 0;
      // Note: numHits is hardcoded to one here (Need to fix!)
      numHits=1;
	  
      minPeakHeight = signal[maxTimes[hitIndex]];
	  
      // consider adding pulse to group of consecutive hits if:
      // 1 less than max consecutive hits
      // 2 we are not at the last point in the signal vector
      // 3 the height of the dip between the two is greater than threshold/2
      // 4 and there is no gap between them
      while(numHits < fMaxMultiHit && numHits+hitIndex < (signed)endTimes.size() && 
	    signal[endTimes[hitIndex+numHits-1]] >threshold/2.0 &&  
	startTimes[hitIndex+numHits] - endTimes[hitIndex+numHits-1] < 2){
	if(signal[maxTimes[hitIndex+numHits]] < minPeakHeight){ 
	  minPeakHeight=signal[maxTimes[hitIndex+numHits]];
	      
	}//<---Only move the mean peakHeight in this hit
	    
	numHits++;//<---Interate the multihit function
	    
      }//<---End multihit while loop
	  
	  
	  
      // ###########################################################	
      // ### Finding the first point from the beginning (startT) ###
      // ###     which is greater then 1/2 the smallest peak     ###
      // ###########################################################
      startT=startTimes[hitIndex];
      while(signal[(int)startT] < minPeakHeight/2.0)  {startT++;}
	  
      // #########################################################	
      // ### Finding the first point from the end (endT) which ###
      // ###      is greater then 1/2 the smallest peak        ###
      // #########################################################
      endT=endTimes[hitIndex+numHits-1];
      while(signal[(int)endT] <minPeakHeight/2.0) {endT--;}
      	
	  
      // ###############################################################
      // ### Putting the current considered hit into a 1-D Histogram ###
      // ###############################################################
      //--- Size of hit = endT - startT ---
      size = (int)(endT-startT);
      
      // ###########################################################################
      // ###    Bug Fix: For ADC counts occuring at the end of the ticks range   ###
      // ### the hitfinder incorrectly assigns the size of the hit as a negative ###
      // ###      number...so we fix this to be 0 so that this hit is skipped    ###
      // ###########################################################################
      if(size < 0){size = 0;}
      

      
      
      std::vector< std::pair<float,int> >mean_matches;
      std::vector< std::pair<float,int> >sigma_matches;
      float prev_dev=0; float slope=0; float sigma=0; float intercept=0; float my_mean=0;
      for(int i = (int)startT; i < (int)endT; i++){
	
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// ~~~ Filling the pulse signals~~~
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	if(i!=(int)startT && i!=(int)endT){
	  slope = (signal[i+1]-signal[i-1])/2/signal[i] - prev_dev;
	  if(slope<0) {
	    sigma = std::sqrt(-1/slope); 
	    intercept = (signal[i+1]-signal[i-1])/2/signal[i] - slope*i;
	    my_mean = -1*intercept/slope;
	  }
	  else if(slope>=0) {
	    sigma=-999; intercept=-999; my_mean=-999;
	  }
	}
	
	//std::cout<<"i = "<<i<<" , signal[i] = "<<signal[i]<< " , derivative[i] = " << (signal[i+1]-signal[i-1])/2/signal[i] 
	//	 << ", slope[i]=" << slope << ", intercept[i]=" << intercept << ", sigma[i]=" << sigma << ", mean[i]=" << my_mean << std::endl;
	prev_dev=(signal[i+1]-signal[i-1])/2/signal[i];
	
	if(my_mean<=0) continue;
	
	bool add_new_element = true;
	for(size_t i_means=0; i_means<mean_matches.size(); i_means++){
	  if( std::abs(my_mean-mean_matches.at(i_means).first)>2.5) continue;
	  else{
	    mean_matches.at(i_means).first = (mean_matches.at(i_means).first*mean_matches.at(i_means).second + my_mean) / (mean_matches.at(i_means).second+1);
	    mean_matches.at(i_means).second+=1;
	    sigma_matches.at(i_means).first = (sigma_matches.at(i_means).first*sigma_matches.at(i_means).second + sigma) / (sigma_matches.at(i_means).second+1);
	    sigma_matches.at(i_means).second+=1;
	    add_new_element=false;
	  }
	}
	if(add_new_element){
	  mean_matches.push_back(std::make_pair(my_mean,1));
	  sigma_matches.push_back(std::make_pair(sigma,1));
	}
	
      }//<---End for looping over startT and endT
      
      int i_means=0;
      std::vector<float> hit_sigmas;
      std::vector<float> hit_means;
      std::vector<float> peak_signals;
      std::vector<int> hit_multiplicity;
      while(i_means < mean_matches.size()){
	//std::cout << "i_mean=" << i_means << "\tmean=" << mean_matches.at(i_means).first << "  multiplicity=" << mean_matches.at(i_means).second << std::endl;
	if(mean_matches.at(i_means).second>=3){
	  hit_means.push_back((mean_matches.at(i_means).first));
	  hit_sigmas.push_back((sigma_matches.at(i_means).first));
	  peak_signals.push_back(signal[(int)(mean_matches.at(i_means).first)]);
	  hit_multiplicity.push_back((mean_matches.at(i_means).second));
	}
	i_means++; 
      }
      
      bool try_reduce=true;
      while(try_reduce){
	try_reduce=false;
	
	for(int i=0; i<hit_means.size(); i++){
	  for(int j=i+1; j<hit_means.size(); j++){
	    
	    if(std::abs(hit_means.at(i) - hit_means.at(j))<2.5){
	      hit_means.at(i) = (hit_means.at(i)*hit_multiplicity.at(i) + hit_means.at(j)*hit_multiplicity.at(j))/(hit_multiplicity.at(i)+hit_multiplicity.at(j));
	      hit_sigmas.at(i) = (hit_sigmas.at(i)*hit_multiplicity.at(i) + hit_sigmas.at(j)*hit_multiplicity.at(j))/(hit_multiplicity.at(i)+hit_multiplicity.at(j));
	      hit_multiplicity.at(i) += hit_multiplicity.at(j);
	      
	      hit_means.erase(hit_means.begin()+j);
	      hit_sigmas.erase(hit_sigmas.begin()+j);
	      hit_multiplicity.erase(hit_multiplicity.begin()+j);
	      peak_signals.erase(peak_signals.begin()+j);
	      
	      try_reduce=true;
	    }
	    
	  }
	}
	
      }
      
      std::vector< std::vector<float> > scaled_distances;
      for(int i=0; i<peak_signals.size(); i++){
	
	std::vector<float> scaled_distance_row;
	float this_mean = hit_means.at(i);
	float this_sigma = hit_sigmas.at(i);
	
	for(int j=0; j<hit_sigmas.size(); j++)
	  scaled_distance_row.push_back( std::abs(this_mean - hit_means.at(j))/hit_sigmas.at(j) );
	
	scaled_distances.push_back(scaled_distance_row);
      }
      
      std::vector<float> solutions = GaussianElimination(scaled_distances,peak_signals);
      
      
      //std::cout << "We have " << solutions.size() << " solutions." << std::endl;
      for(int i=0; i<solutions.size(); i++){


	// get the WireID for this hit
	std::vector<geo::WireID> wids = geom->ChannelToWire(channel);
	///\todo need to have a disambiguation algorithm somewhere in here
	// for now, just take the first option returned from ChannelToWire
	geo::WireID wid = wids[0];
	
	recob::Hit hit(wire, 
		       wid,
		       hit_means.at(i)-hit_sigmas.at(i),
		       hit_sigmas.at(i),
		       hit_means.at(i)+hit_sigmas.at(i),
		       hit_sigmas.at(i),
		       hit_means.at(i),
		       hit_sigmas.at(i),
		       solutions.at(i)*hit_sigmas.at(i)*SQRT_TWO_PI, 
		       0,
		       solutions.at(i),
		       0, 
		       1,     
		       FitGoodnes);   
	hcol->push_back(hit);

      }
      
      hitIndex+=numHits;
      
      
    }//<---End while hitIndex < startTimes.size()
	
    
    
  }//<---End looping over wireIter
  
  
    
  evt.put(std::move(hcol));
  



} // End of produce() 


  DEFINE_ART_MODULE(RFFHitFinder)

} // end of hit namespace
#endif // RFFHitFinder_H
