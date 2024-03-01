/*
 *
 *    Copyright (c) 2021 Christian Holm Christensen
 *    Copyright (c) 2021-2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#ifndef SRC_INCLUDE_SMASH_RIVETOUTPUT_H_
#define SRC_INCLUDE_SMASH_RIVETOUTPUT_H_

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include "Rivet/AnalysisHandler.hh"

#include "smash/fpenvironment.h"
#include "smash/hepmcinterface.h"

namespace smash {

/**
 * \ingroup output
 * \brief SMASH output to Rivet analyses
 *
 * This class passes HepMC events to Rivet for analysis.  The event is kept
 * in-memory which significantly speeds up processing (at last a factor 10)
 * as compared to writing the HepMC event to disk or pipe and then decoding
 * in Rivet.
 *
 * More details of the output format can be found in the User Guide.
 */
class RivetOutput : public HepMcInterface {
 public:
  /**
   * Create Rivet output.
   *
   * \param[in] path Output path.
   * \param[in] name Name of the output.
   * \param[in] full_event Whether the full event or only final-state particles
   *                       are printed in the output
   * \param[in] rivet_par Rivet parameters from SMASH configuration
   */
  RivetOutput(const std::filesystem::path& path, std::string name,
              const bool full_event, const RivetOutputParameters& rivet_par);
  /**
   * Destructor. Finalises the analzyses and writes out results to file
   */
  ~RivetOutput();
  /**
   * Add the final particles information of an event to the central vertex.
   * Store impact parameter and write event.
   *
   * \param[in] particles Current list of particles.
   * \param[in] event_number Number of event.
   * \param[in] event Event info, see \ref event_info
   */
  void at_eventend(const Particles& particles, const int32_t event_number,
                   const EventInfo& event) override;
  /**
   * Add an analysis or analyses to Rivet
   *
   * \param[in] name   Name of analysis
   */
  void add_analysis(const std::string& name);
  /**
   * Add a load path to the Rivet handler
   */
  void add_path(const std::string& path);
  /**
   * Add preload to Rivet handler.
   */
  void add_preload(const std::string& file);
  /**
   * Do not insist on appropriate beams for analyses
   */
  void set_ignore_beams(bool ignore = true);
  /**
   * Set log level in Rivet
   */
  void set_log_level(const std::string& name, const std::string& level);
  /**
   * Set X-section
   *
   * \param[in] xs    Cross-section value in pb
   * \param[in] xserr Uncertainty on cross-section in pb
   */
  void set_cross_section(double xs, double xserr);

 private:
  /**
   * Setup Rivet using SMASH configuration parameters
   *
   * \param[in] params The SMASH parameters already extracted from the
   *                   input configuration
   */
  void setup(const RivetOutputParameters& params);

  /**
   * A proxy object that wraps all Rivet::AnalysisHandler calls in an
   * environment where FP errors are disabled.
   */
  struct Proxy {
    /** Type of handler */
    using Handler = std::shared_ptr<Rivet::AnalysisHandler>;
    /** Construct proxy */
    explicit Proxy(Handler h) : h_(h), g_(0) {}
    /** Move construct */
    Proxy(Proxy&& p) : h_(p.h_), g_(std::move(p.g_)) {}
    /** Forward calls to handler */
    Handler operator->() { return h_; }
    /// Handler
    Handler h_;
    /// FP traps temporaryly disabled
    DisableFloatTraps g_;
  };
  /** Return a proxy that temporarily disables FP exceptions */
  Proxy analysis_handler_proxy() { return Proxy(handler_); }

  /**  Rivet analysis handler */
  std::shared_ptr<Rivet::AnalysisHandler> handler_;
  /** Output file */
  std::filesystem::path filename_;
  /** Whether we need initialisation */
  bool need_init_;
};

}  // namespace smash

#endif  // SRC_INCLUDE_SMASH_RIVETOUTPUT_H_
