///////////////////////////////////////////////////////////////////////
///
/// \file   KalmanFilterAlg.cxx
///
/// \brief  Kalman Filter.
///
/// \author H. Greenlee
///
////////////////////////////////////////////////////////////////////////

#include "TrackFinder/KalmanFilterAlg.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "cetlib/exception.h"

/// Constructor.
  
trkf::KalmanFilterAlg::KalmanFilterAlg(const fhicl::ParameterSet& pset) :
  fTrace(false),
  fPlane(-1),
  fErr(5)
{
  mf::LogInfo("KalmanFilterAlg") << "KalmanFilterAlg instantiated.";

  // Fill starting error matrix.

  fErr.clear();
  fErr(0, 0) = 1000.;
  fErr(1, 1) = 1000.;
  fErr(2, 2) = 10.;
  fErr(3, 3) = 10.;
  fErr(4, 4) = 10.;

  // Load fcl parameters.

  reconfigure(pset);
}

/// Destructor.
trkf::KalmanFilterAlg::~KalmanFilterAlg()
{}

/// Reconfigure method.
void trkf::KalmanFilterAlg::reconfigure(const fhicl::ParameterSet& pset)
{
  fTrace = pset.get<bool>("Trace");
}

/// Add hits to track.
///
/// Arguments:
///
/// trk      - Starting track.
/// trg      - Result global track.
/// prop     - Propagator.
/// dir      - Direction.
/// hits     - Candidate hits.
/// maxChisq - Incremental chisquare cut.
///
/// Returns: True if success.
///
/// This method makes a unidirectional Kalman fit in the specified
/// direction, visiting each surface of the passed KHitContainer and
/// updating the track.  In case of multiple measurements on the same
/// surface, keep (at most) the one with the smallest incremental
/// chisquare.  Any measurements that fail the incremental chisquare
/// cut are rejected.  Resolved hits (accepted or rejected) are moved
/// to the unused list in KHitContainer.  The container is sorted at
/// the start of the method, and may be resorted during the progress
/// of the fit.
///
bool trkf::KalmanFilterAlg::buildTrack(const KTrack& trk,
				       KGTrack& trg,
				       const Propagator* prop,
				       const Propagator::PropDirection dir,
				       KHitContainer& hits,
				       double maxChisq)
{
  assert(prop != 0);

  // Direction must be forward or backward (unknown is not allowed).

  if(dir != Propagator::FORWARD && dir != Propagator::BACKWARD)
    throw cet::exception("KalmanFilterAlg") 
	<< "No direction for Kalman fit.\n";

  // Sort container using this seed track.

  hits.sort(trk, true, prop, dir);

  // Loop over measurements (KHitGroup) from sorted list.

  double tchisq = 0.;        // Cumulative chisquare.
  double path = 0.;          // Cumulative path distance.
  int step = 0;              // Step count.

  // Make a copy of the starting track, in the form of a KFitTrack,
  // which we will update as we go.

  KETrack tre(trk, fErr);
  KFitTrack trf(tre, path, tchisq);

  // Also use the starting track as the reference track for linearized
  // propagation, until the track is established with reasonably small
  // errors.

  KTrack ref(trk);
  KTrack* pref = &ref;

  mf::LogInfo log("KalmanFilterAlg");

  // Loop over measurement groups (KHitGroups).

  while(hits.getSorted().size() > 0) {
    ++step;
    if(fTrace) {
      log << "Build Step " << step << "\n";
      log << "KGTrack has " << trg.getTrackMap().size() << " hits.\n";
      log << trf;
    }

    // Get an iterator for the next KHitGroup.

    std::list<KHitGroup>::iterator it;
    if(dir == Propagator::FORWARD)
      it = hits.getSorted().begin();
    else {
      assert(dir == Propagator::BACKWARD);
      it = hits.getSorted().end();
      --it;
    }
    const KHitGroup& gr = *it;

    if(fTrace) {
      double path_est = gr.getPath();
      log << "Next surface: " << *(gr.getSurface()) << "\n";
      log << "  Estimated distance = " << path_est << "\n";
    }

    // Propagate track to the surface of the KHitGroup.  However, skip
    // propagation if a preferred plane has been set and KHitGroup has
    // a different plane set, or if the seed measurement has not been
    // closed.

    double preddist = 0.;
    boost::optional<double> dist(true, 0.);
    assert(gr.getPlane() >= 0);
    if(fPlane < 0 || gr.getPlane() < 0 || fPlane == gr.getPlane()) {
      boost::shared_ptr<const Surface> psurf = gr.getSurface();
      dist = prop->noise_prop(trf, psurf, Propagator::UNKNOWN, true, pref);
    }
    if(!!dist) {

      // Propagation succeeded.
      // Update cumulative path distance and track status.

      double ds = *dist;
      path += ds;
      trf.setPath(path);
      if(dir == Propagator::FORWARD)
	trf.setStat(KFitTrack::FORWARD_PREDICTED);
      else {
	assert(dir == Propagator::BACKWARD);
	trf.setStat(KFitTrack::BACKWARD_PREDICTED);
      }
      if(fTrace) {
	log << "After propagation\n";
	log << "  Actual distance = " << path << "\n";
	log << "KGTrack has " << trg.getTrackMap().size() << " hits.\n";
	log << trf;
      }

      // Loop over measurements in this group.

      const std::vector<boost::shared_ptr<const KHitBase> >& hits = gr.getHits();
      double best_chisq = 0.;
      boost::shared_ptr<const KHitBase> best_hit;
      for(std::vector<boost::shared_ptr<const KHitBase> >::const_iterator ihit = hits.begin();
	  ihit != hits.end(); ++ihit) {
	const KHitBase& hit = **ihit;

	// Update predction using current track hypothesis and get
	// incremental chisquare.

	bool ok = hit.predict(trf, prop);
	if(ok) {
	  double chisq = hit.getChisq();
	  if(best_hit.get() == 0 || chisq < best_chisq) {
	    best_hit = *ihit;
	    best_chisq = chisq;
	    preddist = hit.getPredDistance();
	  }
	}
      }
      if(fTrace) {
	if(best_hit.get() != 0) {
	  log << "Hit after prediction\n";
	  log << *best_hit;
	}
      }

      // If we found a best measurement, and if the incremental
      // chisquare passes the cut, add it to the track and update 
      // fit information.

      if(best_hit.get() != 0 && best_chisq < maxChisq) {
	best_hit->update(trf);
	tchisq += best_chisq;
	trf.setChisq(tchisq);
	if(dir == Propagator::FORWARD)
	  trf.setStat(KFitTrack::FORWARD);
	else {
	  assert(dir == Propagator::BACKWARD);
	  trf.setStat(KFitTrack::BACKWARD);
	}

	// Make a KHitTrack and add it to the KGTrack.

	KHitTrack trh(trf, best_hit);
	trg.addTrack(trh);

	// Decide if we want to kill the reference track.

	if(trg.getTrackMap().size() >= 30) {
	  pref = 0;
	  if(fTrace)
	    log << "Killing reference track.\n";
	}

	if(fTrace) {
	  log << "After update\n";
	  log << "KGTrack has " << trg.getTrackMap().size() << " hits.\n";
	  log << trf;
	}
      }
    }

    // The current KHitGroup is now resolved.
    // Move it to unused list.

    hits.getUnused().splice(hits.getUnused().end(), hits.getSorted(), it);

    // If the prediction distance was greater than 1 cm, resort the measurements.

    if(std::abs(preddist) > 1.) {
      if(fTrace)
	log << "Resorting measurements.\n";
      hits.sort(trf, false, prop);
    }
  }

  // Set the fit status of the last added KHitTrack to optimal.

  if(dir == Propagator::FORWARD)
    trg.endTrack().setStat(KFitTrack::OPTIMAL);
  else {
    assert(dir == Propagator::BACKWARD);
    trg.startTrack().setStat(KFitTrack::OPTIMAL);
  }

  // Done.  Return success if we added at least one measurement.

  return trg.getTrackMap().size() > 0;
}

/// Smooth track.
///
/// Arguments:
///
/// trg  - Track to be smoothed.
/// trg1 - Track to receive result of unidirectional fit.
/// prop - Propagator.
///
/// Returns: True if success.
///
/// The starting track should be a global track that has been fit in
/// one direction.  Fit status should be optimal at (at least) one
/// end.  It is an error if the fit status is not optimal at either
/// end.  If the fit status is optimal at both ends, do nothing, but
/// return success.
///
/// If the second argument is non-null, save the result of the
/// unidirectional track fit produced as a byproduct of the smoothing
/// operation.  This track can be smoothed in order to iterate the
/// Kalman fit, etc.
///
/// The Kalman smoothing algorithm starts at the optimal end and fits
/// the track in the reverse direction, calculating optimal track
/// parameters at each measurement surface.
///
/// All measurements are included in the reverse fit.  No incremental
/// chisquare cut is applied.
///
/// If any measurement surface can not be reached because of a
/// measurement error, the entire smoothing operation is considered a
/// failure.  In that case, false is returned and the track is left in
/// an undefined state.
///
bool trkf::KalmanFilterAlg::smoothTrack(KGTrack& trg,
					KGTrack* trg1,
					const Propagator* prop)
{
  assert(prop != 0);

  // Default result failure.

  bool result = false;

  // It is an error if the KGTrack is not valid.

  if(trg.isValid()) {

    // Examine the track endpoints and figure out which end of the track
    // to start from.  The fit always starts at the optimal end.  It is
    // an error if neither end point is optimal.  Do nothing and return
    // success if both end points are optimal.

    const KHitTrack& trh0 = trg.startTrack();
    const KHitTrack& trh1 = trg.endTrack();
    KFitTrack::FitStatus stat0 = trh0.getStat();
    KFitTrack::FitStatus stat1 = trh1.getStat();
    bool dofit = false;

    // Remember starting direction, track, and distance.

    Propagator::PropDirection dir = Propagator::UNKNOWN;
    const KTrack* trk = 0;
    double path = 0.;

    if(stat0 == KFitTrack::OPTIMAL) {
      if(stat1 == KFitTrack::OPTIMAL) {

	// Both ends optimal (do nothing, return success).

	dofit = false;
	result = true;

      }
      else {

	// Start optimal.

	dofit = true;
	dir = Propagator::FORWARD;
	trk = &trh0;
	path = trh0.getPath();
      }
    }
    else {
      if(stat1 == KFitTrack::OPTIMAL) {

	// End optimal.

	dofit = true;
	dir = Propagator::BACKWARD;
	trk = &trh1;
	path = trh1.getPath();
      }
      else {

	// Neither end optimal (do nothing, return failure).

	dofit = false;
	result = false;
      }
    }
    if(dofit) {
      assert(dir == Propagator::FORWARD || dir == Propagator::BACKWARD);
      assert(trk != 0);

      // Cumulative chisquare.

      double tchisq = 0.;

      // Construct starting KFitTrack with track information and distance
      // taken from the optimal end, but with "infinite" errors.

      KETrack tre(*trk, fErr);
      KFitTrack trf(tre, path, tchisq);

      // Make initial reference track to be same as initial fit track.

      KTrack ref(trf);

      // Loop over KHitTracks contained in KGTrack.

      std::multimap<double, KHitTrack>::iterator it;
      std::multimap<double, KHitTrack>::iterator itend;
      if(dir == Propagator::FORWARD) {
	it = trg.getTrackMap().begin();
	itend = trg.getTrackMap().end();
      }
      else {
	assert(dir == Propagator::BACKWARD);
	it = trg.getTrackMap().end();
	itend = trg.getTrackMap().begin();
      }

      mf::LogInfo log("KalmanFilterAlg");

      // Loop starts here.

      result = true;             // Result success unless we find an error.
      int step = 0;              // Step count.
      while(dofit && it != itend) {
	++step;
	if(fTrace) {
	  log << "Smooth Step " << step << "\n";
	  log << "Reverse fit track:\n";
	  log << trf;
	}

	// For backward fit, decrement iterator at start of loop.

	if(dir == Propagator::BACKWARD)
	  --it;

	KHitTrack& trh = (*it).second;
	if(fTrace) {
	  log << "Forward track:\n";
	  log << trh;
	}

	// Extract measurement.

	const KHitBase& hit = *(trh.getHit());

	// Propagate KFitTrack to current measurement surface.
	// However, skip propagation if measurement is on a
	// non-preferred plane.
	  
	boost::shared_ptr<const Surface> psurf = trh.getSurface();
	boost::optional<double> dist = prop->noise_prop(trf, psurf, Propagator::UNKNOWN,
							true, &ref);
	if(!dist) {

	  // If propagation failed, abandon the fit and return failure.

	  dofit = false;
	  result = false;
	  break;
	}
	else {

	  // Propagation succeeded.
	  // Update cumulative path distance and track status.

	  double ds = *dist;
	  path += ds;
	  trf.setPath(path);
	  if(dir == Propagator::FORWARD)
	    trf.setStat(KFitTrack::FORWARD_PREDICTED);
	  else {
	    assert(dir == Propagator::BACKWARD);
	    trf.setStat(KFitTrack::BACKWARD_PREDICTED);
	  }
	  if(fTrace) {
	    log << "Reverse fit track after propagation:\n";
	    log << trf;
	  }

	  // See if we have the proper information to calculate an optimal track
	  // at this surface (should normally be possible).

	  KFitTrack::FitStatus stat = trh.getStat();
	  KFitTrack::FitStatus newstat = trf.getStat();

	  if((newstat == KFitTrack::FORWARD_PREDICTED && stat == KFitTrack::BACKWARD) ||
	     (newstat == KFitTrack::BACKWARD_PREDICTED && stat == KFitTrack::FORWARD)) {

	    // Update stored KHitTrack to be optimal.

	    bool ok = trh.combineFit(trf);

	    // Update reference track.

	    ref = trh;

	    // If combination failed, abandon the fit and return failure.

	    if(!ok) {
	      dofit = false;
	      result = false;
	      break;
	    }
	    if(fTrace) {
	      log << "Combined track:\n";
	      log << trh;
	    }
	  }

	  // Update measurement predction using current track hypothesis.

	  bool ok = hit.predict(trf, prop, &ref);
	  if(!ok) {

	    // If prediction failed, abandon the fit and return failure.

	    dofit = false;
	    result = false;
	    break;
	  }
	  else {

	    // Prediction succeeded.
	    // Get incremental chisquare.
	    // Don't make cut, but do update cumulative chisquare.

	    double chisq = hit.getChisq();
	    tchisq += chisq;
	    trf.setChisq(tchisq);

	    // Update the reverse fitting track using the current measurement
	    // (both track parameters and status).

	    hit.update(trf);
	    if(dir == Propagator::FORWARD)
	      trf.setStat(KFitTrack::FORWARD);
	    else {
	      assert(dir == Propagator::BACKWARD);
	      trf.setStat(KFitTrack::BACKWARD);
	    }
	    if(fTrace) {
	      log << "Reverse fit track after update:\n";
	      log << trf;
	    }

	    // If unidirectional track pointer is not null, make a
	    // KHitTrack and save it in the unidirectional track.

	    if(trg1 != 0) {
	      KHitTrack trh1(trf, trh.getHit());
	      trg1->addTrack(trh1);
	    }
	  }
	}

	// For forward fit, increment iterator at end of loop.

	if(dir == Propagator::FORWARD)
	  ++it;

      }    // Loop over KHitTracks.

      // If fit was successful and the unidirectional track pointer
      // is not null, set the fit status of the last added KHitTrack
      // to optimal.

      if(result && trg1 != 0) {
	if(dir == Propagator::FORWARD)
	  trg1->endTrack().setStat(KFitTrack::OPTIMAL);
	else {
	  assert(dir == Propagator::BACKWARD);
	  trg1->startTrack().setStat(KFitTrack::OPTIMAL);
	}
      }
    }      // Do fit.
  }        // KGTrack valid.

  // Done.

  return result;
}

