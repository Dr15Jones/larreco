/**
 * @file   DumpTracks_module.cc
 * @brief  Dumps on screen the content of the tracks
 * @author Gianluca Petrillo (petrillo@fnal.gov)
 * @date   September 12th, 2014
 */

// C//C++ standard libraries
#include <string>
#include <sstream>
#include <iomanip> // std::setw()

// support libraries
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

// art libraries
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

// LArSoft includes
#include "RecoBase/Track.h"


namespace track {

  /**
   * @brief Prints the content of all the tracks on screen
   *
   * This analyser prints the content of all the tracks into the
   * LogInfo/LogVerbatim stream.
   * 
   * <b>Configuration parameters</b>
   * 
   * - <b>TrackModuleLabel</b> (string, required): label of the
   *   producer used to create the recob::Track collection to be dumped
   * - <b>OutputCategory</b> (string, default: "DumpTracks"): the category
   *   used for the output (useful for filtering)
   *
   */
  class DumpTracks : public art::EDAnalyzer {
      public:
    
    /// Default constructor
    explicit DumpTracks(fhicl::ParameterSet const& pset); 
    
    /// Does the printing
    void analyze (const art::Event& evt);

      private:

    std::string fTrackModuleLabel; ///< name of module that produced the tracks
    std::string fOutputCategory; ///< category for LogInfo output

  }; // class DumpTracks

} // End track namespace.


//------------------------------------------------------------------------------
namespace {
  
  /// Returns the length of the string representation of the specified object
  template <typename T>
  size_t StringLength(const T& value) {
    std::ostringstream sstr;
    sstr << value;
    return sstr.str().length();
  } // StringLength()
  
} // local namespace

namespace track {

  //-------------------------------------------------
  DumpTracks::DumpTracks(fhicl::ParameterSet const& pset) 
    : EDAnalyzer         (pset) 
    , fTrackModuleLabel(pset.get<std::string>("TrackModuleLabel"))
    , fOutputCategory    (pset.get<std::string>("OutputCategory", "DumpTracks"))
//    , fHitsPerLine       (pset.get<unsigned int>("HitsPerLine", 20))
    {}


  //-------------------------------------------------
  void DumpTracks::analyze(const art::Event& evt) {
    
    // fetch the data to be dumped on screen
    art::ValidHandle<std::vector<recob::Track>> Tracks
      = evt.getValidHandle<std::vector<recob::Track>>(fTrackModuleLabel);
    
    mf::LogInfo(fOutputCategory)
      << "The event contains " << Tracks->size() << " tracks";
    
    unsigned int iTrack = 0;
    for (const recob::Track& track: *Tracks) {

/*
    void            Extent(std::vector<double> &xyzStart,
                    std::vector<double> &xyzEnd)        const;
    void            Direction(double *dcosStart,
                    double *dcosEnd)                 const;
    double          ProjectedLength(geo::View_t view)          const;
    double          PitchInView(geo::View_t view)              const;


    // A trajectory point is the combination of a position vector
    // and its corresponding direction vector
    size_t          NumberTrajectoryPoints()                    const;
    size_t          NumberCovariance()                          const;
    size_t          NumberFitMomentum()                         const;
    size_t          NumberdQdx(geo::View_t view=geo::kUnknown)  const;
    double          Length(size_t p=0)                          const;
    void            TrajectoryAtPoint(unsigned int  p,
                                      TVector3     &pos,
                                      TVector3     &dir)        const;
    const double&   DQdxAtPoint(unsigned int p,
                                geo::View_t view=geo::kUnknown) const;
    const TVector3& DirectionAtPoint (unsigned int p)           const;
    const TVector3& LocationAtPoint  (unsigned int p)           const;
    const double&   MomentumAtPoint  (unsigned int p)           const;
    const TMatrixD& CovarianceAtPoint(unsigned int p)           const;

    const TVector3& Vertex()                                    const;
    const TVector3& End()                                       const;
    const TVector3& VertexDirection()                           const;
    const TVector3& EndDirection()                              const;
    const TMatrixD& VertexCovariance()                          const;
    const TMatrixD& EndCovariance()                             const;
    const double&   VertexMomentum()                            const;
    const double&   EndMomentum()                               const;

*/
      // print a header for the track
      { // limit the scope of log variable
        mf::LogVerbatim log(fOutputCategory);
        log
          << "Track #" << (iTrack++) << " " << track
          << "ID: " << track.ID()
            << std::fixed << std::setprecision(3)
            << " theta: " << track.Theta() << " phi: " << track.Phi()
          << "\n  direction start: ( " << track.VertexDirection().X()
            << " ; " << track.VertexDirection().Y()
            << " ; " << track.VertexDirection().Z()
            << " ), end: ( " << track.EndDirection().X()
            << " ; " << track.EndDirection().Y()
            << " ; " << track.EndDirection().Z()
            << " )"
          << "\n  with "
            << track.NumberTrajectoryPoints() << " trajectory points, "
            << track.NumberCovariance() << " covariance matrices, dQ/dx"
            << " in views";
        if (track.NumberdQdx(geo::kU) > 0)
          log << " U: " << track.NumberdQdx(geo::kU);
        if (track.NumberdQdx(geo::kV) > 0)
          log << " V: " << track.NumberdQdx(geo::kV);
        if (track.NumberdQdx(geo::kZ) > 0)
          log << " Z: " << track.NumberdQdx(geo::kZ);
        if (track.NumberdQdx(geo::k3D) > 0)
          log << " 3D: " << track.NumberdQdx(geo::k3D);
      } // unnamed block
      
    } // for tracks
    
  } // DumpTracks::analyze()

  DEFINE_ART_MODULE(DumpTracks)

} // namespace track
