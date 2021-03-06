// \file  TrackMomentumCalculator.cxx
// 
// \author sowjanyag@phys.ksu.edu  

#include "larreco/RecoAlg/TrackMomentumCalculator.h"

double my_mcs_chi2( const double *x ) 
{
  Double_t result = 0.0;
  
  Double_t p = x[0]; 
  
  Double_t theta0 = x[1]; 
  
  for ( Int_t i=0; i<nmeas; i++ )
    {
      Double_t xx = xmeas[i]; 
      
      Double_t yy = ymeas[i]; 
      
      Double_t ey = eymeas[i]; 
      
      Double_t rad_length = 14.0;
      
      Double_t l0 = xx/rad_length;
      
      Double_t res1 = 0.0;
      
      if ( xx>0 && p>0 ) res1 = ( 13.6/p )*sqrt( l0 )*( 1.0+0.038*TMath::Log( l0 ) );
      
      res1 = sqrt( res1*res1+theta0*theta0 );
      
      Double_t diff = yy-res1;
      
      if ( ey==0 ) { cout << " Zero denominator in my_mcs_chi2 ! " << endl; return -1; }
      
      Double_t incre = pow( diff/ey, 2.0 );
      
      result += incre;
      
    }
  
  result += 2.0/( 4.6 )*theta0; // *TMath::Log( 1.0/14.0 );
  
  if ( isnan( float(result) ) || isinf( float(result) ) ) { 
    LOG_DEBUG("TrackMomentumCalculator")<<" Is nan in my_mcs_chi2 ! ";
    return -1; }
    
  return result;
  
}

namespace trkf{ 
  
  TrackMomentumCalculator::TrackMomentumCalculator()
  {
    n = 0;
    
    n_reco = 0;
    
    seg_stop = -1.0; n_seg = 0; 
    
    gr_reco_xyz = new TPolyLine3D(); gr_reco_xy = new TGraph(); gr_reco_yz = new TGraph(); gr_reco_xz = new TGraph(); 
    
    gr_seg_xyz = new TPolyLine3D(); gr_seg_xy = new TGraph(); gr_seg_yz = new TGraph(); gr_seg_xz = new TGraph(); 
    
    steps_size = 10.0; n_steps = 6; for ( Int_t i=1; i<=n_steps; i++ ) { steps.push_back( steps_size*i ); }
    
    basex.SetXYZ( 1.0, 0.0, 0.0 ); basey.SetXYZ( 0.0, 1.0, 0.0 ); basez.SetXYZ( 0.0, 0.0, 1.0 ); 
    	 
    nmeas = 0; p_mcs = -1.0; p_mcs_e = -1.0; chi2 = -1.0;
    
    steps_size2 = 10.0;
    
    p_mcs_2 = -1.0; LLbf = -1.0;
    
    kcal = 0.0024;

    minLength = 100;
    
    maxLength = 1350.0;
    
    Float_t Range_grampercm[29] = {9.833E-1, 1.786E0, 3.321E0, 6.598E0, 1.058E1, 3.084E1, 4.250E1, 6.732E1, 1.063E2, 1.725E2, 2.385E2, 4.934E2,                              
                                   6.163E2, 8.552E2, 1.202E3, 1.758E3, 2.297E3,                                                                                             
                                   4.359E3, 5.354E3, 7.298E3, 1.013E4, 1.469E4, 1.910E4, 3.558E4, 4.326E4, 5.768E4, 7.734E4, 1.060E5, 1.307E5};                             
    Float_t KE_MeV[29] = {10, 14, 20, 30, 40, 80, 100, 140, 200, 300, 400, 800, 1000, 1400, 2000, 3000, 4000, 8000, 10000, 14000, 20000, 30000,                              
			  40000, 80000, 100000, 140000, 200000, 300000, 400000};
    KEvsR = new TGraph(29, Range_grampercm, KE_MeV);
    KEvsR_spline3 = new TSpline3("KEvsRS",KEvsR);
    //KEvsRFromData = tfs->make<TH2D>("KEvsRFromData","KE vs R from Data",1000,0,1000,1000,0,5000);
 
  }
  
  double TrackMomentumCalculator::GetTrackMomentum(double trkrange, int pdg) 
  {
   
   /* Muon range-momentum tables from CSDA (Argon density = 1.4 g/cm^3)
   website: http://pdg.lbl.gov/2012/AtomicNuclearProperties/MUON_ELOSS_TABLES/muonloss_289.pdf

   CSDA table values:
   Float_t Range_grampercm[30] = {9.833E-1, 1.786E0, 3.321E0, 6.598E0, 1.058E1, 3.084E1, 4.250E1, 6.732E1, 1.063E2, 1.725E2, 2.385E2, 4.934E2,
   				   6.163E2, 8.552E2, 1.202E3, 1.758E3, 2.297E3,
                                   4.359E3, 5.354E3, 7.298E3, 1.013E4, 1.469E4, 1.910E4, 3.558E4, 4.326E4, 5.768E4, 7.734E4, 1.060E5, 1.307E5};
   Float_t KE_MeV[30] = {10, 14, 20, 30, 40, 80, 100, 140, 200, 300, 400, 800, 1000, 1400, 2000, 3000, 4000, 8000, 10000, 14000, 20000, 30000, 
   			 40000, 80000, 100000, 140000, 200000, 300000, 400000};
			 
   Functions below are obtained by fitting polynomial fits to KE_MeV vs Range (cm) graph. A better fit was obtained
   by splitting the graph into two: Below range<=200cm,a polynomial of power 4 was a good fit; above 200cm, a
   polynomial of power 6 was a good fit
   
   Fit errors for future purposes:
   Below 200cm, Forpoly4 fit: p0 err=1.38533;p1 err=0.209626; p2 err=0.00650077; p3 err=6.42207E-5; p4 err=1.94893E-7;  			 
   Above 200cm, Forpoly6 fit: p0 err=5.24743;p1 err=0.0176229; p2 err=1.6263E-5; p3 err=5.9155E-9; p4 err=9.71709E-13;
   			      p5 err=7.22381E-17;p6 err=1.9709E-21;*/ 
			 
   //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////			      
   //*********For muon, the calculations are valid up to 1.91E4 cm range corresponding to a Muon KE of 40 GeV**********//
   //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   
   /*Proton range-momentum tables from CSDA (Argon density = 1.4 g/cm^3):
   website: http://projects-docdb.fnal.gov:8080/cgi-bin/RetrieveFile?docid=1696;filename=Stopping%20Protons%20-%20July.pdf;version=2
   
   CSDA values:
   Double_t KE_MeV_P_Nist[31]={10, 15, 20, 30, 40, 80, 100, 150, 200, 250, 300, 350, 400, 450, 500, 550, 600, 650, 700, 750, 800, 850, 900, 950, 1000,
                              1500, 2000, 2500, 3000, 4000, 5000};
   
   Double_t Range_gpercm_P_Nist[31]={1.887E-1,3.823E-1, 6.335E-1, 1.296, 2.159, 7.375, 1.092E1, 2.215E1, 3.627E1, 5.282E1, 7.144E1, 9.184E1, 1.138E2,
                                     1.370E2, 1.614E2, 1.869E2, 2.132E2, 2.403E2, 2.681E2, 2.965E2, 3.254E2, 3.548E2, 3.846E2, 4.148E2, 4.454E2, 
				     7.626E2, 1.090E3, 1.418E3, 1.745E3, 2.391E3, 3.022E3}; 

   Functions below are obtained by fitting power and polynomial fits to KE_MeV vs Range (cm) graph. A better fit was obtained
   by splitting the graph into two: Below range<=80cm,a a*(x^b) was a good fit; above 80cm, a
   polynomial of power 6 was a good fit
   
   Fit errors for future purposes:
   For power function fit: a=0.388873; and b=0.00347075		 
   Forpoly6 fit: p0 err=3.49729;p1 err=0.0487859; p2 err=0.000225834; p3 err=4.45542E-7; p4 err=4.16428E-10;
   	         p5 err=1.81679E-13;p6 err=2.96958E-17;*/ 

   ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////			      
   //*********For proton, the calculations are valid up to 3.022E3 cm range corresponding to a Muon KE of 5 GeV**********//
   ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////			      
		 					      			       			 			
   double KE, Momentum, Muon_M = 105.7, Proton_M = 938.272, M;
   
   if (abs(pdg) == 13){
     M = Muon_M;		
     /*if (trkrange>0 && trkrange<=200)
     	  KE = 10.658+ (3.71181*trkrange) + (-0.0302898*trkrange*trkrange) +
	           (0.000228501*trkrange*trkrange*trkrange)+
	           (-5.86201E-7*trkrange*trkrange*trkrange*trkrange);
     else if (trkrange>200 && trkrange<=1.91E4) 	     		
   	  KE = 30.6157+ (2.11089*trkrange) + (0.000255267*trkrange*trkrange)+
	          (-5.19254E-8*trkrange*trkrange*trkrange)+
	          (6.39454E-12*trkrange*trkrange*trkrange*trkrange)+
	          (-3.99392E-16*trkrange*trkrange*trkrange*trkrange*trkrange)+
	          (9.73174E-21*trkrange*trkrange*trkrange*trkrange*trkrange*trkrange);		  
      else
      KE = -999;*/
     KE = KEvsR_spline3->Eval(trkrange);
      } 	  
   else if (abs(pdg) == 2212){
      M = Proton_M;
      if (trkrange>0 && trkrange<=80)
   	 KE = 29.9317*TMath::Power(trkrange,0.586304);
      else if (trkrange>80 && trkrange<=3.022E3)	
   	 KE = 149.904+ (3.34146*trkrange) + (-0.00318856*trkrange*trkrange)+
	     (4.34587E-6*trkrange*trkrange*trkrange)+
	     (-3.18146E-9*trkrange*trkrange*trkrange*trkrange)+
	     (1.17854E-12*trkrange*trkrange*trkrange*trkrange*trkrange)+
	     (-1.71763E-16*trkrange*trkrange*trkrange*trkrange*trkrange*trkrange);
      else
         KE = -999;
      }
    else
      KE = -999;   
      	  	     
    if (KE<0) 	     			  
       Momentum = -999;  
    else
       Momentum = TMath::Sqrt((KE*KE)+(2*M*KE));
       
    Momentum = Momentum/1000;  
       
    return Momentum;       	  		 	     			  
  }
  
  /*double TrackMomentumCalculator::GetProtonTrackMomentum(double trkrange) 
  {	 		
   double ProtonKE;		
   if (trkrange<=80)
   	ProtonKE = 29.9317*TMath::Power(trkrange,0.586304);
   else	
   	ProtonKE = 149.904+ (3.34146*trkrange) + (-0.00318856*trkrange*trkrange)+
	     (4.34587E-6*trkrange*trkrange*trkrange)+
	     (-3.18146E-9*trkrange*trkrange*trkrange*trkrange)+
	     (1.17854E-12*trkrange*trkrange*trkrange*trkrange*trkrange)+
	     (-1.71763E-16*trkrange*trkrange*trkrange*trkrange*trkrange*trkrange);
	     			  
    return ProtonKE;
  }*/
  
  // Momentum measurement via Multiple Coulomb Scattering (MCS)
  
  // author: Leonidas N. Kalousis (July 2015)
  
  // email: kalousis@vt.edu
  
  Double_t TrackMomentumCalculator::GetMomentumMultiScatterLLHD( const art::Ptr<recob::Track> &trk )
  {
    Double_t p = -1.0; 
    
    std::vector<Float_t> recoX; std::vector<Float_t> recoY; std::vector<Float_t> recoZ;
    
    recoX.clear(); recoY.clear(); recoZ.clear();
    
    Int_t n_points = trk->NumberTrajectoryPoints();
        
    for ( Int_t i=0; i<n_points; i++ )
      {
	const TVector3 &pos = trk->LocationAtPoint( i );
	
	recoX.push_back( pos.X() ); recoY.push_back( pos.Y() ); recoZ.push_back( pos.Z() ); 
	
	// cout << " posX, Y, Z : " << pos.X() << ", " << pos.Y() << ", " << pos.Z() << endl; getchar();
	
      }
    
    Int_t my_steps = recoX.size();
    
    if ( my_steps<2 ) return -1.0;
    
    Int_t check0 = GetRecoTracks( recoX, recoY, recoZ );
    
    if ( check0!=0 ) return -1.0;
        
    seg_size = steps_size2; 
    
    Int_t check1 = GetSegTracks2( recoX, recoY, recoZ ); 
    
    if ( check1!=0 ) return -1.0;
        
    Int_t seg_steps = segx.size();
    
    if ( seg_steps<2 ) return -1;
    
    Int_t seg_steps0 = seg_steps-1;
    
    Double_t recoL = segL.at(seg_steps0);
    
    if ( recoL<minLength || recoL>maxLength ) return -1;
    
    Int_t check2 = GetDeltaThetaij( dEi, dEj, dthij, seg_size, ind ); 
    
    if ( check2!=0 ) return -1.0;
    
    Double_t logL = 1e+16; 
    
    Double_t bf = -666.0; // Double_t errs = -666.0;
        
    Double_t start1 = 0.0; Double_t end1 = 750.0; 
	      
    Double_t start2 = 0.0; Int_t end2 = 0.0; // 800.0; 
        
    for ( Int_t k=start1; k<=end1; k++ )
      {
	Double_t p_test = 0.001+k*0.01; 
	
	for ( Int_t l=start2; l<=end2; l++ )
	  {
	    Double_t res_test = 2.0; // 0.001+l*1.0; 
	    
	    Double_t fv = my_mcs_llhd( p_test, res_test ); 
	    	    
	    if ( fv<logL )
	      {
		bf = p_test;
		
		logL = fv;
		
		// errs = res_test;
		
	      }
	    
	  }
	
      }
    
    p_mcs_2 = bf; LLbf = logL;
    
    p = p_mcs_2;
    
    return p;
    
  }
  
  TVector3 TrackMomentumCalculator::GetMultiScatterStartingPoint( const art::Ptr<recob::Track> &trk )
  {
    TVector3 start;
    
    Double_t LLHDp = GetMuMultiScatterLLHD3( trk, true );
    
    Double_t LLHDm = GetMuMultiScatterLLHD3( trk, false );
    
    if ( LLHDp!=-1 && LLHDm!=-1 )
      {
	if ( LLHDp>LLHDm )
	  {
	    Int_t n_points = trk->NumberTrajectoryPoints();
	    
	    const TVector3 &pos = trk->LocationAtPoint( n_points-1 );
		
	    start.SetXYZ( pos.X(), pos.Y(), pos.Z() );
	    
	  }
	else 
	  { 
	    const TVector3 &pos = trk->LocationAtPoint( 0 );
	    
	    start.SetXYZ( pos.X(), pos.Y(), pos.Z() );
	    
	  }
        
      }
    
    else 
      { 
	const TVector3 &pos = trk->LocationAtPoint( 0 );
		
	start.SetXYZ( pos.X(), pos.Y(), pos.Z() );
	
      }
    
    return start;
    
  }
  /*
  Double_t TrackMomentumCalculator::GetMuMultiScatterLLHD( const art::Ptr<recob::Track> &trk )
  {
    Double_t LLHD = -1.0; 
    
    std::vector<Float_t> recoX; std::vector<Float_t> recoY; std::vector<Float_t> recoZ;
    
    recoX.clear(); recoY.clear(); recoZ.clear();
    
    Int_t n_points = trk->NumberTrajectoryPoints();
    
    for ( Int_t i=0; i<n_points; i++ )
      {
	const TVector3 &pos = trk->LocationAtPoint( i );
	
	recoX.push_back( pos.X() ); recoY.push_back( pos.Y() ); recoZ.push_back( pos.Z() ); 
	
	// cout << " posX, Y, Z : " << pos.X() << ", " << pos.Y() << ", " << pos.Z() << endl;
	
      }
    
    Int_t my_steps = recoX.size();
    
    if ( my_steps<2 ) return -1.0;
    
    Int_t check0 = GetRecoTracks( recoX, recoY, recoZ );
    
    if ( check0!=0 ) return -1.0;
    
    seg_size = 5.0; 
    
    Int_t check1 = GetSegTracks2( recoX, recoY, recoZ );
    
    if ( check1!=0 ) return -1.0;
        
    Int_t seg_steps = segx.size();
    
    if ( seg_steps<2 ) return -1;
    
    Int_t seg_steps0 = seg_steps-1;
    
    Double_t recoL = segL.at(seg_steps0);
    
    if ( recoL<15.0 || recoL>1350.0 ) return -1;
        
    Int_t check2 = GetDeltaThetaij( dEi, dEj, dthij, seg_size, ind ); 
    
    if ( check2!=0 ) return -1.0;
    
    Double_t p_range = recoL*kcal;
    
    Double_t logL = my_mcs_llhd( p_range, 0.5 ); 
    
    LLHD = logL;
    
    return LLHD;
    
  }
  
  Double_t TrackMomentumCalculator::GetMuMultiScatterLLHD2( const recob::Track &trk )
  {
    Double_t LLHD = -1.0; 
    
    std::vector<Float_t> recoX; std::vector<Float_t> recoY; std::vector<Float_t> recoZ;
    
    recoX.clear(); recoY.clear(); recoZ.clear();
   
    Int_t n_points = trk.NumberTrajectoryPoints();
        
    for ( Int_t i=0; i<n_points; i++ )
      {
	const TVector3 &pos = trk.LocationAtPoint( i );
	
	recoX.push_back( pos.X() ); recoY.push_back( pos.Y() ); recoZ.push_back( pos.Z() ); 
	
	// cout << " posX, Y, Z : " << pos.X() << ", " << pos.Y() << ", " << pos.Z() << endl;
	
      }
    
    Int_t my_steps = recoX.size();
    
    if ( my_steps<2 ) return -1.0;
    
    Int_t check0 = GetRecoTracks( recoX, recoY, recoZ );
    
    if ( check0!=0 ) return -1.0;
    
    seg_size = 5.0;
    
    Int_t check1 = GetSegTracks2( recoX, recoY, recoZ );
    
    if ( check1!=0 ) return -1.0;
        
    Int_t seg_steps = segx.size();
    
    if ( seg_steps<2 ) return -1;
    
    Int_t seg_steps0 = seg_steps-1;
    
    Double_t recoL = segL.at(seg_steps0);
    
    if ( recoL<15.0 || recoL>1350.0 ) return -1;
        
    Int_t check2 = GetDeltaThetaij( dEi, dEj, dthij, seg_size, ind ); 
    
    if ( check2!=0 ) return -1.0;
    
    Double_t p_range = recoL*kcal;
    
    Double_t logL = my_mcs_llhd( p_range, 0.5 ); 
    
    LLHD = logL;
    
    return LLHD;
    
  }
  */
  Double_t TrackMomentumCalculator::GetMuMultiScatterLLHD3( const art::Ptr<recob::Track> &trk , bool dir )
  {
    Double_t LLHD = -1.0; 
    
    std::vector<Float_t> recoX; std::vector<Float_t> recoY; std::vector<Float_t> recoZ;
    
    recoX.clear(); recoY.clear(); recoZ.clear();
    
    Int_t n_points = trk->NumberTrajectoryPoints();
        
    if ( dir )
      {
	for ( Int_t i=0; i<n_points; i++ )
	  {
	    const TVector3 &pos = trk->LocationAtPoint( i );
	    
	    recoX.push_back( pos.X() ); recoY.push_back( pos.Y() ); recoZ.push_back( pos.Z() ); 
	    
	    // cout << " posX, Y, Z : " << pos.X() << ", " << pos.Y() << ", " << pos.Z() << endl;
	    
	  }
	
      }
    else 
      {
	for ( Int_t i=0; i<n_points; i++ )
	  {
	    const TVector3 &pos = trk->LocationAtPoint( n_points-1-i );
	    
	    recoX.push_back( pos.X() ); recoY.push_back( pos.Y() ); recoZ.push_back( pos.Z() ); 
	    
	    // cout << " posX, Y, Z : " << pos.X() << ", " << pos.Y() << ", " << pos.Z() << endl;
	    
	  }
	
      }
    
    Int_t my_steps = recoX.size();
    
    if ( my_steps<2 ) return -1.0;
    
    Int_t check0 = GetRecoTracks( recoX, recoY, recoZ );
    
    if ( check0!=0 ) return -1.0;
    
    seg_size = 5.0;
    
    Int_t check1 = GetSegTracks2( recoX, recoY, recoZ );
    
    if ( check1!=0 ) return -1.0;
        
    Int_t seg_steps = segx.size();
    
    if ( seg_steps<2 ) return -1;
    
    Int_t seg_steps0 = seg_steps-1;
    
    Double_t recoL = segL.at(seg_steps0);
    
    if ( recoL<15.0 || recoL>maxLength ) return -1;
        
    Int_t check2 = GetDeltaThetaij( dEi, dEj, dthij, seg_size, ind ); 
    
    if ( check2!=0 ) return -1.0;
    
    Double_t p_range = recoL*kcal;
    
    Double_t logL = my_mcs_llhd( p_range, 5.65 );
    
    LLHD = logL;
    
    return LLHD;
    
  }
  
  Int_t TrackMomentumCalculator::GetDeltaThetaij( std::vector<Float_t> &ei, std::vector<Float_t> &ej, std::vector<Float_t> &th, Double_t thick, std::vector<Float_t> &ind )
  {
    Int_t a1 = segx.size(); Int_t a2 = segy.size(); Int_t a3 = segz.size();
        
    if ( ( a1!=a2 ) || ( a1!=a3 ) ) { cout << " ( Get thij ) Error ! " << endl; return -1.0; }
    
    Int_t tot = a1-1; Double_t thick1 = thick+0.13;
    
    ei.clear(); ej.clear(); th.clear(); ind.clear();
    
    azx0.clear(); azy0.clear();
    
    for ( Int_t i=0; i<tot; i++ )
      {
	Double_t dx = segnx.at( i ); Double_t dy = segny.at( i ); Double_t dz = segnz.at( i );
	
	TVector3 vec_z( dx, dy, dz ); 
			
	TVector3 vec_x; 
	
	TVector3 vec_y; 
	
	Double_t switcher = basex.Dot( vec_z ); 
	
	if ( TMath::Abs( switcher )<=0.995 ) 
	  {
	    vec_y = vec_z.Cross( basex ); vec_y = vec_y.Unit();  
	    
	    vec_x = vec_y.Cross( vec_z );
	    	    
	  }
	
	else 
	  {
	    // cout << " It switched ! Isn't this lovely !!! " << endl; // getchar();
	    
	    vec_y = basez.Cross( vec_z ); vec_y = vec_y.Unit();  
	    
	    vec_x = vec_y.Cross( vec_z );
	    	    	    
	  }
	
	TVector3 Rx;
	
	Double_t ex = vec_x.Dot( basex ); Rx.SetX( ex );
	
	ex = vec_x.Dot( basey ); Rx.SetY( ex );
	
	ex = vec_x.Dot( basez ); Rx.SetZ( ex );
	
	TVector3 Ry;
	
	Double_t ey = vec_y.Dot( basex ); Ry.SetX( ey );
  
	ey = vec_y.Dot( basey ); Ry.SetY( ey );
	
	ey = vec_y.Dot( basez ); Ry.SetZ( ey );
	
	TVector3 Rz;
	
	Double_t ez = vec_z.Dot( basex ); Rz.SetX( ez );
	
	ez = vec_z.Dot( basey ); Rz.SetY( ez );
	
	ez = vec_z.Dot( basez ); Rz.SetZ( ez );
		
	Double_t refL = segL.at( i );
	
	for ( Int_t j=i; j<tot; j++ )
	  {
	    Double_t L1 = segL.at( j );
	    
	    Double_t L2 = segL.at( j+1 );
	    	    	    
	    Double_t dz1 = L1 - refL; 
	    
	    Double_t dz2 = L2 - refL; 
	    
	    if ( dz1<=thick1 && dz2>thick1 )
	      {
		Double_t here_dx = segnx.at( j );
		
		Double_t here_dy = segny.at( j );
		
		Double_t here_dz = segnz.at( j );
		
		Double_t ac1 =	0.001*180.0/3.14159*find_angle( here_dz, here_dx );
		
		Double_t ac2 =	0.001*180.0/3.14159*find_angle( here_dz, here_dy );
		
		TVector3 here_vec; here_vec.SetXYZ( here_dx, here_dy, here_dz );
						
		TVector3 rot_here; rot_here.SetXYZ( Rx.Dot( here_vec ), Ry.Dot( here_vec ), Rz.Dot( here_vec ) );
						
		Double_t scx = rot_here.X(); 
	    
		Double_t scy = rot_here.Y();  
		
		Double_t scz = rot_here.Z();  
		
		Double_t azy = find_angle( scz, scy );
		
		Double_t azx = find_angle( scz, scx );
		
		Double_t ULim = 10000.0; 

                Double_t LLim = -10000.0;
		
		Double_t cL = kcal;
		
		Double_t Li = segL.at( i );
		
		Double_t Lj = segL.at( j );
				
		if ( azy<=ULim && azy>=LLim ) 
		  { 
		    ei.push_back( Li*cL ); 
		    
		    ej.push_back( Lj*cL );
		    
		    th.push_back( azy );
		    
		    ind.push_back( 2 );
		    
		    azx0.push_back( ac1 );
		    
		    azy0.push_back( ac2 );
		    
		  }
		
		if ( azx<=ULim && azx>=LLim ) 
		  { 
		    ei.push_back( Li*cL ); 
 
		    ej.push_back( Lj*cL );
		    
		    th.push_back( azx );
		    
		    ind.push_back( 1 );
		    
		    azx0.push_back( ac1 );
		    
		    azy0.push_back( ac2 );
		    
		  }
		
		break; // of course !
		
	      }
	
	  }
	
      }
    
    return 0;
    
  }
  
  Double_t TrackMomentumCalculator::GetMomentumMultiScatterChi2( const art::Ptr<recob::Track> &trk )
  {
    Double_t p = -1.0; 
    
    std::vector<Float_t> recoX; std::vector<Float_t> recoY; std::vector<Float_t> recoZ;
    
    recoX.clear(); recoY.clear(); recoZ.clear();
        	  
    Int_t n_points = trk->NumberTrajectoryPoints();
        
    for ( Int_t i=0; i<n_points; i++ )
      {
	const TVector3 &pos = trk->LocationAtPoint( i );
	
	recoX.push_back( pos.X() ); recoY.push_back( pos.Y() ); recoZ.push_back( pos.Z() ); 
	
	// cout << " posX, Y, Z : " << pos.X() << ", " << pos.Y() << ", " << pos.Z() << endl;
	
      }
    
    Int_t my_steps = recoX.size();
    
    if ( my_steps<2 ) return -1.0;
    
    Int_t check0 = GetRecoTracks( recoX, recoY, recoZ );
    
    if ( check0!=0 ) return -1.0;
    
    seg_size = steps_size;
    
    Int_t check1 = GetSegTracks2( recoX, recoY, recoZ );
    
    if ( check1!=0 ) return -1.0;
        
    Int_t seg_steps = segx.size();
    
    if ( seg_steps<2 ) return -1;
    
    Int_t seg_steps0 = seg_steps-1;
    
    Double_t recoL = segL.at(seg_steps0);
    
    if ( seg_steps<2 || recoL<minLength || recoL>maxLength ) return -1;
        
    Double_t mean = 666.0; Double_t rms = 666.0; Double_t rmse = 666.0;
    
    nmeas = 0; Double_t max1=-999.0; Double_t min1=+999.0;
    
    for ( Int_t j=0; j<n_steps; j++ )
      {
	Double_t trial = steps.at( j );
	
	GetDeltaThetaRMS( mean, rms, rmse, trial );
	
	// cout << mean << ",  " << rms << ", " << rmse << ", " << trial << endl;
    
	xmeas[ nmeas ] = trial;
    
	ymeas[ nmeas ] = rms;
    
	eymeas[ nmeas ] = sqrt( pow( rmse, 2.0 ) + pow( 0.05*rms, 2.0 ) ); // <--- conservative syst. error to fix chi^{2} behaviour !!!
    
	// ymeas[ nmeas ] = 10.0; eymeas[ nmeas ] = 1.0; // <--- for debugging !
    
	if ( min1>ymeas[ nmeas ] ) min1=ymeas[ nmeas ];
    
	if ( max1<ymeas[ nmeas ] ) max1=ymeas[ nmeas ];
    
	nmeas++;
    
      }
    
    gr_meas = new TGraphErrors( nmeas, xmeas, ymeas, 0, eymeas );
    	      
    gr_meas->SetTitle( "(#Delta#theta)_{rms} versus material thickness; Material thickness in cm; (#Delta#theta)_{rms} in mrad" );
    
    gr_meas->SetLineColor( kBlack ); gr_meas->SetMarkerColor( kBlack ); gr_meas->SetMarkerStyle( 20 ); gr_meas->SetMarkerSize( 1.2 ); 
    
    gr_meas->GetXaxis()->SetLimits( ( steps.at( 0 )-steps.at( 0 ) ), ( steps.at( n_steps-1 )+steps.at( 0 ) ) );
    
    gr_meas->SetMinimum( 0.0 );
    
    gr_meas->SetMaximum( 1.80*max1 );
    
    // c1->cd();
    
    // gr_meas->Draw( "APEZ" );
    
    // c1->Update();
    
    // c1->WaitPrimitive();
    
    ROOT::Minuit2::Minuit2Minimizer *mP = new ROOT::Minuit2::Minuit2Minimizer( );
    
    ROOT::Math::Functor FCA( &my_mcs_chi2, 2 ); 
    
    mP->SetFunction( FCA );
        
    mP->SetLimitedVariable( 0, "p_{MCS}", 1.0, 0.01, 0.001, 7.5 ); 
    
    mP->SetLimitedVariable( 1, "#delta#theta", 0.0, 1.0, 0.0, 45.0 ); 
	 
    mP->SetMaxFunctionCalls( 1.E9 );
    
    mP->SetMaxIterations( 1.E9 );
    
    mP->SetTolerance( 0.01 );
    
    mP->SetStrategy( 2 );
    
    mP->SetErrorDef( 1.0 );
    
    bool mstatus = mP->Minimize();
    
    mP->Hesse();
    
    const double *pars = mP->X();  
    
    const double *erpars = mP->Errors(); 
    
    Double_t deltap = ( recoL*kcal )/2.0;
    
    p_mcs = pars[0]+deltap;
	      	      
    p_mcs_e = erpars[0];
    
    if ( mstatus ) p = p_mcs;
    
    else p = -1.0;
    
    chi2 = mP->MinValue(); 
    
    delete mP;
    
    return p;
    
  }
  
  Int_t TrackMomentumCalculator::GetTracks( const std::vector<Float_t> &xxx, const std::vector<Float_t> &yyy, const std::vector<Float_t> &zzz )
  {
    Int_t a1 = xxx.size(); Int_t a2 = yyy.size(); Int_t a3 = zzz.size();
    
    if ( ( a1!=a2 ) || ( a1!=a3 ) || ( a2!=a3 ) ) { cout << " ( Get tracks ) Error ! " << endl; return -1; }
    
    n = 0;
    
    for ( Int_t i=0; i<a1; i++ )
      {
	Double_t nowx = xxx.at( i ); 
	
	Double_t nowy = yyy.at( i ); 
	
	Double_t nowz = zzz.at( i ); 
	
	x[n] = nowx;
	
	y[n] = nowy;
	
	z[n] = nowz;
	
	n++;
	
      }
    
    gr_xyz = new TPolyLine3D( n, z, x, y );
    
    gr_yz = new TGraph( n, z, y ); gr_xz = new TGraph( n, z, x ); gr_xy = new TGraph( n, x, y );
    
    return 0;
    
  }
  
  Int_t TrackMomentumCalculator::GetRecoTracks( const std::vector<Float_t> &xxx, const std::vector<Float_t> &yyy, const std::vector<Float_t> &zzz )
  {
    Int_t a1 = xxx.size(); Int_t a2 = yyy.size(); Int_t a3 = zzz.size();
    
    if ( ( a1!=a2 ) || ( a1!=a3 ) || ( a2!=a3 ) ) { cout << " ( Get reco tacks ) Error ! " << endl; return -1; }
    
    n_reco = 0;
    
    for ( Int_t i=0; i<a1; i++ )
      {
	Double_t nowx = xxx.at( i ); 
	
	Double_t nowy = yyy.at( i ); 
	
	Double_t nowz = zzz.at( i ); 
		
	x_reco[n_reco] = nowx;
		
	y_reco[n_reco] = nowy;
	
	z_reco[n_reco] = nowz;
	
	n_reco++;
	
      }
    
    gr_reco_xyz = new TPolyLine3D( n_reco, z_reco, x_reco, y_reco );
    
    gr_reco_yz = new TGraph( n_reco, z_reco, y_reco ); gr_reco_xz = new TGraph( n_reco, z_reco, x_reco ); gr_reco_xy = new TGraph( n_reco, x_reco, y_reco );
    
    return 0;
    
  }
  
  Int_t TrackMomentumCalculator::GetSegTracks( const std::vector<Float_t> &xxx, const std::vector<Float_t> &yyy, const std::vector<Float_t> &zzz )
  {
    Int_t a1 = xxx.size(); Int_t a2 = yyy.size(); Int_t a3 = zzz.size();
    
    if ( ( a1!=a2 ) || ( a1!=a3 ) || ( a2!=a3 ) ) { cout << " ( Digitize reco tacks ) Error ! " << endl; return -1; } 
    
    Int_t stopper = seg_stop / seg_size; 
    	  
    Int_t a4 = a1-1;
        
    segx.clear(); segy.clear(); segz.clear(); 
    
    segnx.clear(); segny.clear(); segnz.clear(); 
    
    segL.clear(); 
            
    Double_t x0 = xxx.at( 0 ); Double_t y0 = yyy.at( 0 ); Double_t z0 = zzz.at( 0 );
    
    segx.push_back( x0 ); segy.push_back( y0 ); segz.push_back( z0 ); segL.push_back( 0.0 );
    
    n_seg = 0; x_seg[ n_seg ] = x0; y_seg[ n_seg ] = y0; z_seg[ n_seg ] = z0;
    
    n_seg++;
    
    Int_t ntot = 0; 
    
    Double_t Sz = 0; Double_t Szz = 0;
    
    Double_t Szx = 0; Double_t Sx = 0;
    
    Double_t Szy = 0; Double_t Sy = 0;
    
    Sz += z0; Szz += z0*z0;
    
    Szx += z0*x0; Sx += x0;
    
    Szy += z0*y0; Sy += y0;
        
    ntot++;
            
    for ( Int_t i=1; i<a4; i++ )
      {
	Double_t x1 = xxx.at( i ); Double_t y1 = yyy.at( i ); Double_t z1 = zzz.at( i );
	
	Double_t dr1 = sqrt( pow( x1-x0, 2 )+pow( y1-y0, 2)+pow( z1-z0, 2 ) ); 
	
	Double_t x2 = xxx.at( i+1 ); Double_t y2 = yyy.at( i+1 ); Double_t z2 = zzz.at( i+1 );
	
	Double_t dr2 = sqrt( pow( x2-x0, 2 )+pow( y2-y0, 2)+pow( z2-z0, 2 ) ); 
	
	if ( dr1<seg_size )
	  {
	    Sz += z1; Szz += z1*z1;
    
	    Szx += z1*x1; Sx += x1;
    
	    Szy += z1*y1; Sy += y1;
	    
	    ntot++;
    
	  }
	
	if ( dr1<=seg_size && dr2>seg_size )
	  {
	    Double_t t = 0.0;
	    
	    Double_t dx = x2-x1; Double_t dy = y2-y1; Double_t dz = z2-z1;
	    
	    Double_t dr = sqrt( dx*dx+dy*dy+dz*dz );
	    
	    if ( dr==0 ) { cout << " ( Zero ) Error ! " << endl; return -1; }
	    
	    Double_t beta = 2.0*( (x1-x0)*dx+(y1-y0)*dy+(z1-z0)*dz )/( dr*dr );
	    	    
	    Double_t gamma = ( dr1*dr1 - seg_size*seg_size )/( dr*dr );
	    
	    Double_t delta = beta*beta - 4.0*gamma;
		
	    if ( delta<0.0 ) { cout << " ( Discriminant ) Error ! " << endl; return -1; }
		
	    Double_t lysi1 = ( -beta+sqrt( delta ) )/2.0; 
	    
	    t=lysi1;
	    	    
	    Double_t xp = x1+t*dx;
	    
	    Double_t yp = y1+t*dy;
	    
	    Double_t zp = z1+t*dz;
	    
	    segx.push_back( xp ); segy.push_back( yp ); segz.push_back( zp );
	    
	    segL.push_back( 1.0*n_seg*1.0*seg_size );
	    
	    x_seg[ n_seg ] = xp; y_seg[ n_seg ] = yp; z_seg[ n_seg ] = zp; n_seg++; 
	    	    
	    x0 = xp; y0 = yp; z0 = zp;
	    	    	     
	    Sz += z0; Szz += z0*z0;
	    
	    Szx += z0*x0; Sx += x0;
	    
	    Szy += z0*y0; Sy += y0;
	    
	    ntot++;
	    
	    Double_t ax = ( Szx*ntot-Sz*Sx )/( Szz*ntot-Sz*Sz );
	      
	    Double_t ay = ( Szy*ntot-Sz*Sy )/( Szz*ntot-Sz*Sz );
	    
	    segnx.push_back( ax ); segny.push_back( ay ); segnz.push_back( 1.0 );
	    	    
	    ntot = 0; 
    
	    Sz = 0; Szz = 0; 
	    
	    Szx = 0; Sx = 0; 
	    
	    Szy = 0; Sy = 0;
	    
	    Sz += z0; Szz += z0*z0;
	     
	    Szx += z0*x0; Sx += x0;
	    
	    Szy += z0*y0; Sy += y0;
	    
	    ntot++;
	    	    
	  }
	
	else if ( dr1>seg_size )
	  {
	    Double_t t = 0.0;
	    	    
	    Double_t dx = x1-x0; Double_t dy = y1-y0; Double_t dz = z1-z0;
	    	    
	    Double_t dr = sqrt( dx*dx+dy*dy+dz*dz );
	    
	    if ( dr==0 ) { cout << " ( Zero ) Error ! " << endl; getchar(); }
	    
	    if ( dr!=0 ) t = seg_size/dr;
	    	    	    
	    Double_t xp = x0+t*dx;
	    
	    Double_t yp = y0+t*dy;
	    
	    Double_t zp = z0+t*dz;
	    	    	    	    
	    segx.push_back( xp ); segy.push_back( yp ); segz.push_back( zp );
	    
	    segL.push_back( 1.0*n_seg*1.0*seg_size );
	    
	    x_seg[ n_seg ] = xp; y_seg[ n_seg ] = yp; z_seg[ n_seg ] = zp; n_seg++; 
	    	    
	    x0 = xp; y0 = yp; z0 = zp;
	    
	    i=( i-1 );
	    	    
	    Sz += z0; Szz += z0*z0;
	    
	    Szx += z0*x0; Sx += x0;
	    
	    Szy += z0*y0; Sy += y0;
	    
	    ntot++;
	    
	    Double_t ax = ( Szx*ntot-Sz*Sx )/( Szz*ntot-Sz*Sz );
	      
	    Double_t ay = ( Szy*ntot-Sz*Sy )/( Szz*ntot-Sz*Sz );
	    
	    segnx.push_back( ax ); segny.push_back( ay ); segnz.push_back( 1.0 );
	    
	    // Double_t angx = find_angle( 1.0, ax ); Double_t angy = find_angle( 1.0, ay );
	    
	    // cout << angx*0.001*180.0/3.14 << endl;
	    
	    ntot = 0; 
	    
	    Sz = 0; Szz = 0; 
	    
	    Szx = 0; Sx = 0; 
	    
	    Szy = 0; Sy = 0;
	    
	    Sz += z0; Szz += z0*z0;
	     
	    Szx += z0*x0; Sx += x0;
	    
	    Szy += z0*y0; Sy += y0;
	    
	    ntot++;
	    	    
	  }
	
	if ( n_seg>=( stopper+1.0 ) && seg_stop!=-1 ) break;
		
      }
    
    gr_seg_xyz = new TPolyLine3D( n_seg, z_seg, x_seg, y_seg );
    
    gr_seg_yz = new TGraph( n_seg, z_seg, y_seg ); gr_seg_xz = new TGraph( n_seg, z_seg, x_seg ); gr_seg_xy = new TGraph( n_seg, x_seg, y_seg );
    
    return 0;
  
  }
  
  Int_t TrackMomentumCalculator::GetSegTracks2( const std::vector<Float_t> &xxx, const std::vector<Float_t> &yyy, const std::vector<Float_t> &zzz )
  {
    Double_t stag = 0.0;
      
    Int_t a1 = xxx.size(); Int_t a2 = yyy.size(); Int_t a3 = zzz.size();
    
    if ( ( a1!=a2 ) || ( a1!=a3 ) || ( a2!=a3 ) ) { cout << " ( Digitize reco tacks ) Error ! " << endl; return -1; }
    
    Int_t stopper = seg_stop / seg_size; 
    	  
    Int_t a4 = a1-1;
    
    segx.clear(); segy.clear(); segz.clear(); 
    
    segnx.clear(); segny.clear(); segnz.clear(); 
    
    segL.clear(); 
            
    Int_t ntot = 0; 
    
    n_seg = 0;
    
    Double_t x0=0.0; Double_t y0=0.0; Double_t z0=0.0; 
    
    Double_t x00 = xxx.at( 0 ); Double_t y00 = yyy.at( 0 ); Double_t z00 = zzz.at( 0 );
    
    Int_t indC=0;
        
    std::vector<Float_t> vx; std::vector<Float_t> vy; std::vector<Float_t> vz;
    
    vx.clear(); vy.clear(); vz.clear();

    for ( Int_t i=0; i<=a4; i++ )
      {
	x0 = xxx.at( i ); y0 = yyy.at( i ); z0 = zzz.at( i );
	
	Double_t RR0 = sqrt( pow(x00-x0,2.0)+pow(y00-y0,2.0)+pow(z00-z0,2.0) );
	
	if ( RR0>=stag )
	  {
	    segx.push_back( x0 ); segy.push_back( y0 ); segz.push_back( z0 );
	    
	    segL.push_back( stag );
	    
	    x_seg[ n_seg ] = x0; y_seg[ n_seg ] = y0; z_seg[ n_seg ] = z0;
	    
	    n_seg++;
	    
	    vx.push_back( x0 ); vy.push_back( y0 ); vz.push_back( z0 );
	    
	    ntot++;
	    
	    indC = i+1; 
	    
	    break;
	    
	  }

      }
    
    for ( Int_t i=indC; i<a4; i++ )
      {
	Double_t x1 = xxx.at( i ); Double_t y1 = yyy.at( i );	Double_t z1 = zzz.at( i );
	
	Double_t dr1 = sqrt( pow( x1-x0, 2 )+pow( y1-y0, 2)+pow( z1-z0, 2 ) ); 
	
	Double_t x2 = xxx.at( i+1 ); Double_t y2 = yyy.at( i+1 ); Double_t z2 = zzz.at( i+1 );
	
	Double_t dr2 = sqrt( pow( x2-x0, 2 )+pow( y2-y0, 2)+pow( z2-z0, 2 ) ); 
	
	if ( dr1<seg_size )
	  {
	    vx.push_back( x1 ); vy.push_back( y1 ); vz.push_back( z1 );
	    
	    ntot++;
    
	  }
	
	if ( dr1<=seg_size && dr2>seg_size )
	  {
	    // ..
	    
	    // cout << " 1 " << endl;
	    
	    Double_t t = 0.0;
	    
	    Double_t dx = x2-x1; Double_t dy = y2-y1; Double_t dz = z2-z1;
	    
	    Double_t dr = sqrt( dx*dx+dy*dy+dz*dz );
	    
	    if ( dr==0 ) { cout << " ( Zero ) Error ! " << endl; return -1; }
	    
	    Double_t beta = 2.0*( (x1-x0)*dx+(y1-y0)*dy+(z1-z0)*dz )/( dr*dr );
	    	    
	    Double_t gamma = ( dr1*dr1 - seg_size*seg_size )/( dr*dr );
	    
	    Double_t delta = beta*beta - 4.0*gamma;
		
	    if ( delta<0.0 ) { cout << " ( Discriminant ) Error ! " << endl; return -1; }
		
	    Double_t lysi1 = ( -beta+sqrt( delta ) )/2.0; 
	    	    
	    t=lysi1;
	    	    
	    Double_t xp = x1+t*dx;
	    
	    Double_t yp = y1+t*dy;
	    
	    Double_t zp = z1+t*dz;
	    
	    segx.push_back( xp ); segy.push_back( yp ); segz.push_back( zp );
	    
	    segL.push_back( 1.0*n_seg*1.0*seg_size+stag );
	    
	    x_seg[ n_seg ] = xp; y_seg[ n_seg ] = yp; z_seg[ n_seg ] = zp; n_seg++; 
	    
	    x0 = xp; y0 = yp; z0 = zp;
	    
	    vx.push_back( x0 ); vy.push_back( y0 ); vz.push_back( z0 );
	    	    
	    ntot++;
	    
	    Double_t na = vx.size();

	    Double_t sumx = 0.0;
	    
	    Double_t sumy = 0.0;
	    
	    Double_t sumz = 0.0;
	    
	    for ( Int_t ii=0; ii<na; ii++ )
	      {
		Double_t xxw1 = vx.at( ii ); 
		
		Double_t yyw1 = vy.at( ii );
		
		Double_t zzw1 = vz.at( ii );
		
		sumx += xxw1; sumy += yyw1; sumz += zzw1;
		
	      }
	    
	    sumx = sumx/na; sumy = sumy/na; sumz = sumz/na;
	    
	    std::vector<Double_t> mx;
	    
	    std::vector<Double_t> my;
	    
	    std::vector<Double_t> mz;
	    
	    TMatrixDSym m( 3 );
	    
	    for ( Int_t ii=0; ii<na; ii++ )
	      {
		Double_t xxw1 = vx.at( ii ); Double_t yyw1 = vy.at( ii ); Double_t zzw1 = vz.at( ii );
		
		mx.push_back( xxw1-sumx ); my.push_back( yyw1-sumy ); mz.push_back( zzw1-sumz );
		
		Double_t xxw0 = mx.at( ii ); Double_t yyw0 = my.at( ii ); Double_t zzw0 = mz.at( ii );
		
		m( 0, 0 ) += xxw0*xxw0/na; m( 0, 1 ) += xxw0*yyw0/na; m( 0, 2 ) += xxw0*zzw0/na;
      
		m( 1, 0 ) += yyw0*xxw0/na; m( 1, 1 ) += yyw0*yyw0/na; m( 1, 2 ) += yyw0*zzw0/na;
		
		m( 2, 0 ) += zzw0*xxw0/na; m( 2, 1 ) += zzw0*yyw0/na; m( 2, 2 ) += zzw0*zzw0/na;
		
	      }
	    	    
	    TMatrixDSymEigen me(m);
	    
	    TVectorD eigenval = me.GetEigenValues();
	    
	    TMatrixD eigenvec = me.GetEigenVectors();
	    	    
	    Double_t max1 = -666.0;
   
	    Double_t ind1 = 0;
	    
	    for ( Int_t ii=0; ii<3; ii++ )
	      {
		Double_t p1 = eigenval( ii );
		
		if ( p1>max1 ) { max1=p1; ind1=ii; }
		
	      }
	    
	    // cout << ind1 << endl;
	    
	    Double_t ax = eigenvec( 0, ind1 );
	    
	    Double_t ay = eigenvec( 1, ind1 ); 
	    
	    Double_t az = eigenvec( 2, ind1 );
	    
	    if ( n_seg>1 )
	      {
		if ( segx.at(n_seg-1)-segx.at(n_seg-2) > 0 ) ax = TMath::Abs( ax );
		
		else ax = -1.0*TMath::Abs( ax );
		
		if ( segy.at(n_seg-1)-segy.at(n_seg-2) > 0 ) ay = TMath::Abs( ay );
		
		else ay = -1.0*TMath::Abs( ay );
		
		if ( segz.at(n_seg-1)-segz.at(n_seg-2) > 0 ) az = TMath::Abs( az );
		
		else az = -1.0*TMath::Abs( az );
		
		segnx.push_back( ax ); segny.push_back( ay ); segnz.push_back( az );
		
		// Double_t angx = find_angle( 1.0, ax ); Double_t angy = find_angle( 1.0, ay );
		
		// cout << angx*0.001*180.0/3.14 << endl;
		
	      }
	    
	    else return -1;
	    
	    ntot = 0; 
	    
	    vx.clear(); vy.clear(); vz.clear();
	    
	    vx.push_back( x0 ); vy.push_back( y0 ); vz.push_back( z0 );
	    
	    ntot++;
	    	    
	  }
	
	else if ( dr1>seg_size )
	  {
	    // ..
	    
	    // cout << " 2 " << endl;
	    
	    Double_t t = 0.0;
	    	    
	    Double_t dx = x1-x0; Double_t dy = y1-y0; Double_t dz = z1-z0;
	    	    
	    Double_t dr = sqrt( dx*dx+dy*dy+dz*dz );
	    
	    if ( dr==0 ) { cout << " ( Zero ) Error ! " << endl; return -1; }
	    
	    if ( dr!=0 ) t = seg_size/dr;
	    	    	    
	    Double_t xp = x0+t*dx;
	    
	    Double_t yp = y0+t*dy;
	    
	    Double_t zp = z0+t*dz;
	    	    	    	    
	    segx.push_back( xp ); segy.push_back( yp ); segz.push_back( zp );
	    
	    segL.push_back( 1.0*n_seg*1.0*seg_size+stag );
	    
	    x_seg[ n_seg ] = xp; y_seg[ n_seg ] = yp; z_seg[ n_seg ] = zp; n_seg++; 
	    	    
	    x0 = xp; y0 = yp; z0 = zp;
	    
	    i=( i-1 );
	    
	    // ..
	    
	    vx.push_back( x0 ); vy.push_back( y0 ); vz.push_back( z0 );
	    	    
	    ntot++;
	    	    
	    Double_t na = vx.size();

	    Double_t sumx = 0.0;
	    
	    Double_t sumy = 0.0;
	    
	    Double_t sumz = 0.0;
	    
	    for ( Int_t ii=0; ii<na; ii++ )
	      {
		Double_t xxw1 = vx.at( ii ); 
		
		Double_t yyw1 = vy.at( ii );
		
		Double_t zzw1 = vz.at( ii );
		
		sumx += xxw1; sumy += yyw1; sumz += zzw1;
		
	      }
	    
	    sumx = sumx/na; sumy = sumy/na; sumz = sumz/na;
	    
	    std::vector<Double_t> mx;
	    
	    std::vector<Double_t> my;
	    
	    std::vector<Double_t> mz;
	    
	    TMatrixDSym m( 3 );
	    
	    for ( Int_t ii=0; ii<na; ii++ )
	      {
		Double_t xxw1 = vx.at( ii ); Double_t yyw1 = vy.at( ii ); Double_t zzw1 = vz.at( ii );
		
		mx.push_back( xxw1-sumx ); my.push_back( yyw1-sumy ); mz.push_back( zzw1-sumz );
		
		Double_t xxw0 = mx.at( ii ); Double_t yyw0 = my.at( ii ); Double_t zzw0 = mz.at( ii );
		
		m( 0, 0 ) += xxw0*xxw0/na; m( 0, 1 ) += xxw0*yyw0/na; m( 0, 2 ) += xxw0*zzw0/na;
      
		m( 1, 0 ) += yyw0*xxw0/na; m( 1, 1 ) += yyw0*yyw0/na; m( 1, 2 ) += yyw0*zzw0/na;
		
		m( 2, 0 ) += zzw0*xxw0/na; m( 2, 1 ) += zzw0*yyw0/na; m( 2, 2 ) += zzw0*zzw0/na;
		
	      }
	    	    
	    TMatrixDSymEigen me(m);
	    
	    TVectorD eigenval = me.GetEigenValues();
	    
	    TMatrixD eigenvec = me.GetEigenVectors();
	    	    
	    Double_t max1 = -666.0;
   
	    Double_t ind1 = 0;
	    
	    for ( Int_t ii=0; ii<3; ii++ )
	      {
		Double_t p1 = eigenval( ii );
		
		if ( p1>max1 ) { max1=p1; ind1=ii; }
		
	      }
	    
	    // cout << ind1 << endl;
	    	    
	    Double_t ax = eigenvec( 0, ind1 );
	    
	    Double_t ay = eigenvec( 1, ind1 ); 
	    
	    Double_t az = eigenvec( 2, ind1 );
	    
	    if ( n_seg>1 )
	      {
		
		if ( segx.at(n_seg-1)-segx.at(n_seg-2) > 0 ) ax = TMath::Abs( ax );
		
		else ax = -1.0*TMath::Abs( ax );
		
		if ( segy.at(n_seg-1)-segy.at(n_seg-2) > 0 ) ay = TMath::Abs( ay );
		
		else ay = -1.0*TMath::Abs( ay );
		
		if ( segz.at(n_seg-1)-segz.at(n_seg-2) > 0 ) az = TMath::Abs( az );
		
		else az = -1.0*TMath::Abs( az );
		
		segnx.push_back( ax ); segny.push_back( ay ); segnz.push_back( az );
		
		// Double_t angx = find_angle( 1.0, ax ); Double_t angy = find_angle( 1.0, ay );
		
		// cout << angx*0.001*180.0/3.14 << endl;
		
	      }
	    
	    else return -1;
	    
	    ntot = 0; 
	    
	    vx.clear(); vy.clear(); vz.clear();
	    
	    vx.push_back( x0 ); vy.push_back( y0 ); vz.push_back( z0 );
	    
	    ntot++;
	    	    	    
	  }
	
	if ( n_seg>=( stopper+1.0 ) && seg_stop!=-1 ) break;
	
      }
    
    gr_seg_xyz = new TPolyLine3D( n_seg, z_seg, x_seg, y_seg );
    
    gr_seg_yz = new TGraph( n_seg, z_seg, y_seg ); gr_seg_xz = new TGraph( n_seg, z_seg, x_seg ); gr_seg_xy = new TGraph( n_seg, x_seg, y_seg );
    
    return 0;
  
  }
  
  void TrackMomentumCalculator::GetDeltaThetaRMS( Double_t &mean, Double_t &rms, Double_t &rmse, Double_t thick )
  {
    mean = 0.0; rms = 0.0; rmse = 0.0; 
    
    Int_t a1 = segx.size(); Int_t a2 = segy.size(); Int_t a3 = segz.size();
        
    if ( ( a1!=a2 ) || ( a1!=a3 ) ) { cout << " ( Get RMS ) Error ! " << endl; return; }
        
    Int_t tot = a1-1;
    
    Double_t thick1 = thick+0.13;
    
    vector<Float_t> buf0; buf0.clear();
    
    for ( Int_t i=0; i<tot; i++ )
      {
	Double_t dx = segnx.at( i ); Double_t dy = segny.at( i ); Double_t dz = segnz.at( i );
	
	TVector3 vec_z( dx, dy, dz ); 
			
	TVector3 vec_x; 
	
	TVector3 vec_y; 
	
	Double_t switcher = basex.Dot( vec_z ); 
	
	if ( switcher<=0.995 ) 
	  {
	    vec_y = vec_z.Cross( basex ); vec_y = vec_y.Unit();  
	    
	    vec_x = vec_y.Cross( vec_z );
	    	    
	  }
	
	else 
	  {
	    // cout << " It switched ! Isn't this lovely !!! " << endl; // getchar();
	    
	    vec_y = basez.Cross( vec_z ); vec_y = vec_y.Unit();  
	    
	    vec_x = vec_y.Cross( vec_z );
	    	    	    
	  }
	
	TVector3 Rx;
	
	Double_t ex = vec_x.Dot( basex ); Rx.SetX( ex );
	
	ex = vec_x.Dot( basey ); Rx.SetY( ex );
	
	ex = vec_x.Dot( basez ); Rx.SetZ( ex );
	
	TVector3 Ry;
	
	Double_t ey = vec_y.Dot( basex ); Ry.SetX( ey );
  
	ey = vec_y.Dot( basey ); Ry.SetY( ey );
	
	ey = vec_y.Dot( basez ); Ry.SetZ( ey );
	
	TVector3 Rz;
	
	Double_t ez = vec_z.Dot( basex ); Rz.SetX( ez );
	
	ez = vec_z.Dot( basey ); Rz.SetY( ez );
	
	ez = vec_z.Dot( basez ); Rz.SetZ( ez );
		
	Double_t refL = segL.at( i );
	
	for ( Int_t j=i; j<tot; j++ )
	  {
	    Double_t L1 = segL.at( j );
	    
	    Double_t L2 = segL.at( j+1 );
	    	    	    
	    Double_t dz1 = L1 - refL; 
	    
	    Double_t dz2 = L2 - refL; 
	    
	    if ( dz1<=thick1 && dz2>thick1 )
	      {
	        Double_t here_dx = segnx.at( j );
		
		Double_t here_dy = segny.at( j );
		
		Double_t here_dz = segnz.at( j );
		
		TVector3 here_vec; here_vec.SetXYZ( here_dx, here_dy, here_dz );
						
		TVector3 rot_here; rot_here.SetXYZ( Rx.Dot( here_vec ), Ry.Dot( here_vec ), Rz.Dot( here_vec ) );
						
		Double_t scx = rot_here.X(); 
	    
		Double_t scy = rot_here.Y();  
		
		Double_t scz = rot_here.Z();  
		
		Double_t azy = find_angle( scz, scy ); azy*=1.0;
		
		Double_t azx = find_angle( scz, scx );
		
		Double_t ULim = 10000.0; Double_t LLim = -10000.0;
		
		// if ( azy<=ULim && azy>=LLim ) { buf0.push_back( azy ); } // hRMS.Fill( azy ); }
		
		if ( azx<=ULim && azx>=LLim ) { buf0.push_back( azx ); } // hRMS.Fill( azx ); }

		// if ( azy<=ULim && azy>=LLim && azx<=ULim && azx>=LLim && thick==5.0 ) { hSCA.Fill( azx, azy ); }
		
		// i=j-1;
		
		break; // of course !
		
	      }
	  }
     
      }
    
    Int_t nmeas = buf0.size(); Double_t nnn = 0.0;
    
    for ( Int_t i=0; i<nmeas; i++ ) { mean += buf0.at( i ); nnn++; } mean = mean/nnn;
    
    for ( Int_t i=0; i<nmeas; i++ ) rms += ( ( buf0.at( i ) )*( buf0.at( i ) ) );
    
    rms = rms/( nnn ); rms = sqrt( rms ); rmse = rms/sqrt( 2.0*tot );
    
    Double_t rms1 = rms;
    
    rms = 0.0;
    
    Double_t ntot1 = 0.0;
    
    Double_t lev1 = 2.50;
 
    for ( Int_t i=0; i<nmeas; i++ ) 
      {
	Double_t amp = buf0.at( i );
		
	if ( amp<( mean+lev1*rms1 ) && amp>( mean-lev1*rms1 )  ) 
	  {
	    ntot1++;
	    
	    rms += ( amp*amp );
	    
	  }
      
      }
    
    rms = rms/( ntot1 ); rms = sqrt( rms ); rmse = rms/sqrt( 2.0*ntot1 );
            
    return;
    
  }
  
  Double_t TrackMomentumCalculator::find_angle( Double_t vz, Double_t vy )
  {
    Double_t thetayz = -999.0;
    
    if ( vz>0 && vy>0 ) { Double_t ratio=TMath::Abs( vy/vz ); thetayz=TMath::ATan( ratio ); }
    
    else if ( vz<0 && vy>0 ) { Double_t ratio=TMath::Abs( vy/vz ); thetayz=TMath::ATan( ratio ); thetayz=3.14159-thetayz; } 
    
    else if ( vz<0 && vy<0 ) { Double_t ratio=TMath::Abs( vy/vz ); thetayz=TMath::ATan( ratio ); thetayz=thetayz+3.14159; }
    
    else if ( vz>0 && vy<0 ) { Double_t ratio=TMath::Abs( vy/vz ); thetayz=TMath::ATan( ratio ); thetayz=2.0*3.14159-thetayz; } 
    
    else if ( vz==0 && vy>0 ) { thetayz=3.14159/2.0; } 
    
    else if ( vz==0 && vy<0 ) { thetayz=3.0*3.14159/2.0; } 
    
    if ( thetayz>3.14159 ) { thetayz=thetayz-2.0*3.14159; } 
    
    Double_t result = 1000.0*thetayz;
    
    return result; 
    
  }
    
  Double_t TrackMomentumCalculator::my_g( Double_t xx, Double_t Q, Double_t s )
  {
    Double_t arg = 0.0; 
    
    if ( s!=0 ) arg = ( xx - Q )/s;    
    
    else cout << " Error : The code tries to divide by zero ! " << endl;
    
    Double_t result = 0.0; 
    
    if ( s!=0 ) result = -0.5*TMath::Log( 2.0*TMath::Pi() ) - TMath::Log( s ) - 0.5*arg*arg;
    
    if ( isnan( float( result ) ) || isinf( float( result ) ) ) { 
      cout << " Is nan ! my_g ! " << - TMath::Log( s ) << ", " << s << endl; 
      getchar(); 
    }
    
    return result;
    
  }
  
  Double_t TrackMomentumCalculator::my_mcs_llhd( Double_t x0, Double_t x1 )
  {
    Double_t p = x0;
    
    Double_t theta0x = x1;
    
    Double_t result = 0.0;
    
    Double_t nnn1 = dEi.size();
    
    Double_t red_length = ( 10.0 )/14.0;
    
    Double_t addth = 0;
    
    for ( Int_t i=0; i<nnn1; i++ )
      {
	Double_t Ei = p-dEi.at( i );
	
	Double_t Ej = p-dEj.at( i );
	
	if ( Ei>0 && Ej<0 ) addth=3.14*1000.0;
	
	Ei = TMath::Abs( Ei );
	
	Ej = TMath::Abs( Ej );
	
	Double_t tH0 = ( 13.6/sqrt( Ei*Ej ) )*( 1.0+0.038*TMath::Log( red_length ) )*sqrt( red_length );
	
	Double_t rms = -1.0;
	
	if ( ind.at( i )==1 )
	  {
	    rms = sqrt( tH0*tH0+pow( theta0x, 2.0 ) );
	    
	    Double_t DT = dthij.at( i )+addth; 
	    
	    Double_t prob = my_g( DT, 0.0, rms ); 
	    
	    result = result - 2.0*prob; 
	    
	  }
	
      }
    
    if ( isnan( float( result ) ) || isinf( float( result ) ) ) { cout << " Is nan ! my_mcs_llhd ( 1 ) ! " << endl; getchar(); }
    
    return result;
    
  }
    
} // namespace track
