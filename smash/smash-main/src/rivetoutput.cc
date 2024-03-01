/*
 *
 *    Copyright (c) 2021 Christian Holm Christensen
 *    Copyright (c) 2021-2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "smash/rivetoutput.h"

#include "Rivet/Rivet.hh"
#include "Rivet/Tools/Logging.hh"

#include "smash/logging.h"
#include "smash/outputparameters.h"

namespace smash {
/*!\Userguide
 * \page doxypage_output_rivet
 *
 * SMASH Rivet output interface directly to Rivet analyses on the events
 * generated by SMASH.  That is, instead of writing the events to
 * disk (or pipe) this output module generates HepMC (see also \ref
 * doxypage_output_hepmc) events in-memory and pass them on
 * directly to Rivet to avoid the costly encoding and decoding of HepMC3
 * events to and from disk (or pipe). This provides a speed up (at least
 * \f$\times 10\f$) compared to intermediate files.
 *
 * The results of the analyses are written to YODA files.
 *
 * \section rivet_output_user_guide_format_ Internal event formats
 *
 * Internally, this output module generates HepMC events.  These can
 * be made in two ways, selected by the formats \key YODA and \key
 * YODA-full.
 *
 * - \key YODA In this format only initial (beam) and final state
 *   particles are stored in the event structure and there is only
 *   one interaction point. Note, the incoming nucleons are combined
 *   into single nuclei.
 *
 * - \key YODA-full In this format, the whole event is stored
 *   (including intermediate states). Note, the incoming nucleons
 *   are combined into single nuclei, only to be split at the main
 *   interaction point. This allows tracking of the individual
 *   nucleons.
 *
 * Refer to https://yoda.hepforge.org/ for further details.
 * Only one format can be chosen. SMASH will use the first format recognized as
 * valid, ignoring the rest.
 * Please, note that choosing YODA or YODA-full determines the kind of
 * information available to the analysis, but the content of the final YODA
 * files depends on the analysis itself.
 * Depending on what it does, the analysis might work fine with both formats,
 * nevertheless, if it is not necessary to know the structure of the whole
 * event, it is recommended to choose the lighter YODA format, thus saving
 * computational time and resources (especially the RAM).
 *
 * \section rivet_output_user_guide_config_ Configuration
 *
 * The Rivet process can be configured in the main configuration
 * file.  This is done by adding keys to the \key Rivet key.  Here, we can
 *
 * - Set load paths for data and analyses (\key Paths)
 * - Preload data files - e.g., centrality calibrations (\key Preloads)
 * - Specify which analyses to run (\key Analyses)
 * - Choose whether to not validate beams (\key Ignore_Beams)
 * - Set log levels in Rivet (\key Logging)
 * - Specify weight handling (\key Weights)
 * - and more ... (see below)
 *
 * An example:
 * \verbatim
 Output:
    Rivet:
      Format: ["YODA"]
      Ignore_Beams: True
      Logging:
        Rivet.AnalysisHandler: Debug
        Rivet.Analysis.MC_FSPARTICLES: Debug
      Analyses:
        - MC_FSPARTICLES
 \endverbatim
 *
 * The Rivet set-up can be configured using the \ref input_output_rivet_
 * "content specific \c Rivet section" in the configuration file.
 */

RivetOutput::RivetOutput(const std::filesystem::path& path, std::string name,
                         const bool full_event,
                         const RivetOutputParameters& rivet_par)
    : HepMcInterface(name, full_event),
      handler_(),
      filename_(path / (name + ".yoda")),
      need_init_(true) {
  handler_ = std::make_shared<Rivet::AnalysisHandler>();
  setup(rivet_par);
}

RivetOutput::~RivetOutput() {
  logg[LOutput].debug() << "Writing Rivet results to " << filename_
                        << std::endl;
  analysis_handler_proxy()->finalize();
  analysis_handler_proxy()->writeData(filename_.string());
}

void RivetOutput::at_eventend(const Particles& particles,
                              const int32_t event_number,
                              const EventInfo& event) {
  HepMcInterface::at_eventend(particles, event_number, event);

  // Initialize Rivet on first event
  if (need_init_) {
    logg[LOutput].debug() << "Initialising Rivet" << std::endl;
    need_init_ = false;
    analysis_handler_proxy()->init(event_);
  }

  logg[LOutput].debug() << "Analysing event " << event_number << std::endl;
  // Let Rivet analyse the event
  analysis_handler_proxy()->analyze(event_);
}

void RivetOutput::add_analysis(const std::string& name) {
  analysis_handler_proxy()->addAnalysis(name);
}

void RivetOutput::add_path(const std::string& path) {
  Rivet::addAnalysisLibPath(path);
  Rivet::addAnalysisDataPath(path);
}

void RivetOutput::add_preload(const std::string& file) {
  analysis_handler_proxy()->readData(file);
}

void RivetOutput::set_ignore_beams(bool ignore) {
  logg[LOutput].info() << "Ignore beams? " << (ignore ? "yes" : "no")
                       << std::endl;
  analysis_handler_proxy()->setIgnoreBeams(ignore);
}

void RivetOutput::set_log_level(const std::string& name,
                                const std::string& level) {
  std::string fname(name);
  if (fname.rfind("Rivet", 0) != 0) {
    fname = "Rivet." + fname;
  }

  auto upcase = [](const std::string& s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](char c) { return std::toupper(c); });
    return out;
  };

  try {
    Rivet::Log::setLevel(fname, Rivet::Log::getLevelFromName(upcase(level)));
  } catch (...) {
  }
}

void RivetOutput::set_cross_section(double xs, double xserr) {
  analysis_handler_proxy()->setCrossSection(xs, xserr, true);
}

void RivetOutput::setup(const RivetOutputParameters& params) {
  logg[LOutput].debug() << "Setting up Rivet output:\n";

  // Paths to analyses libraries and data
  if (params.paths) {
    logg[LOutput].info() << "Processing paths\n";
    for (auto p : params.paths.value())
      add_path(p);
  }

  // Data files to pre-load e.g., for centrality configurations
  if (params.preloads) {
    logg[LOutput].info() << "Processing preloads\n";
    for (auto p : params.preloads.value())
      add_preload(p);
  }

  // Analyses (including options) to add to run
  if (params.analyses) {
    logg[LOutput].info() << "Processing analyses\n";
    for (auto p : params.analyses.value())
      add_analysis(p);
  }

  // Whether Rivet should ignore beams
  set_ignore_beams(params.ignore_beams);

  // Cross sections
  if (params.cross_sections) {
    set_cross_section(params.cross_sections.value()[0],
                      params.cross_sections.value()[1]);
  }

  // Logging in Rivet
  if (params.logs) {
    for (auto nl : params.logs.value())
      set_log_level(nl.first, nl.second);
  }

  // Treatment of event weights in Rivet
  if (params.any_weight_parameter_was_given) {
    // Do not care about multi weights
    if (params.no_multi_weight) {
      analysis_handler_proxy()->skipMultiWeights(
          params.no_multi_weight.value());
    }

    // Set nominal weight name
    if (params.nominal_weight_name) {
      analysis_handler_proxy()->setNominalWeightName(
          params.nominal_weight_name.value());
    }

    // Set cap (maximum) on weights
    if (params.cap_on_weights) {
      analysis_handler_proxy()->setWeightCap(params.cap_on_weights.value());
    }

    // Whether to smear for NLO calculations
    if (params.nlo_smearing) {
      analysis_handler_proxy()->setNLOSmearing(params.nlo_smearing.value());
    }

    // Select which weights to enable
    if (params.to_be_enabled_weights) {
      std::stringstream s;
      int comma = 0;
      for (auto w : params.to_be_enabled_weights.value())
        s << (comma++ ? "," : "") << w;
      analysis_handler_proxy()->selectMultiWeights(s.str());
    }

    // Select weights to disable
    if (params.to_be_disabled_weights) {
      std::stringstream s;
      int comma = 0;
      for (auto w : params.to_be_disabled_weights.value())
        s << (comma++ ? "," : "") << w;
      analysis_handler_proxy()->deselectMultiWeights(s.str());
    }
  }
  logg[LOutput].debug() << "Setup of Rivet output done.\n";
}
}  // namespace smash