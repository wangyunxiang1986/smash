/*
 *
 *    Copyright (c) 2019-2020,2022-2023
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "smash/icoutput.h"

#include <filesystem>
#include <fstream>

#include "smash/action.h"

namespace smash {
static constexpr int LHyperSurfaceCrossing = LogArea::HyperSurfaceCrossing::id;

/*!\Userguide
 * \page doxypage_output_initial_conditions
 * The ASCII initial conditions output (SMASH_IC.dat) contains a list of
 * particles on a hypersurface of constant proper time. This output is formatted
 * such that it is directly compatible with the
 * vHLLE hydrodynamics code (I. Karpenko, P. Huovinen, M.
 * Bleicher: Comput. Phys. Commun. 185, 3016 (2014)). As a consequence,
 * **spectators are not written to the ASCII IC output** as they would need to
 * be excluded anyways in order to initialize the hydrodynamics evolution. Note
 * though that for all other output formats the full particle list is printed to
 * the IC output, including spectators. The particle data is provided in the
 * computational frame. For further details, see \ref
 * doxypage_output_initial_conditions. \n
 *
 * \n
 * The ASCII initial conditions output is formatted as follows:
 * \n
 * **Header**
 * \code
 * # **smash_version** initial conditions: hypersurface of constant proper time
 * # tau x y eta mt px py Rap pdg charge baryon_number strangeness
 * # fm fm fm none GeV GeV GeV none none e none none
 * \endcode
 * The header consists of 3 lines starting with a '#', containing the following
 * information:
 * -# SMASH-version and the information, that 'initial conditions' are provided
 * -# The header with all column names
 * -# The units of all column quantities
 *
 * **Output block header**
 *
 * The ASCII initial conditions output is, similar to the OSCAR output, based on
 * a block structure, where each block consists of 1 event. The header for a
 * new event is structured as follows:
 * \code
 * # event ev_num start
 * \endcode
 * where
 * \li \key ev_num: The number of the current event
 *
 * Note that 'event' and 'start' are no variables, but words that are
 * printed in the header. \n
 * \n
 * **Particle line**
 *
 * The particle lines are formatted as follows:
 * \code
 * tau x y eta mt px py Rap pdg charge baryon_number strangeness
 * \endcode
 * where
 * \li \key tau: Proper time of the particle
 * \li \key x, \key y: Cartesian x and y coordinates of the particle
 * \li \key eta: Space-time rapidity of the particle
 * \li \key mt: Transverse mass of the particle
 * \li \key px, \key py: x and y components of the particle's momentum
 * \li \key Rap: Momentum space rapidity of the particle
 * \li \key pdg: PDG code of the particle (see http://pdg.lbl.gov/).
 * It contains all quantum numbers and uniquely identifies its type.
 * every particle in the event.
 * \li \key charge: electric charge of the particle
 * \li \key baryon_number: baryon number of the particle
 * \li \key strangeness: strangeness of the particle
 *
 * **Event end line**
 *
 * The end of an event is indicated by the following line:
 * \code
 * # event ev_num end
 * \endcode
 * where
 * \li \key ev_num: The number of the current event
 *
 * Note that 'event' and 'end' are no variables, but words that are
 * printed in the header. \n
 *
 * \note
 * If SMASH is run with test particles (necessary e.g. for potentials), the
 * ASCII output will contain Ntest * Npart particle entries. Remember to weigh
 * each of those particles with 1/Ntest.
 */

ICOutput::ICOutput(const std::filesystem::path &path, const std::string &name,
                   const OutputParameters &out_par)
    : OutputInterface(name),
      file_{path / "SMASH_IC.dat", "w"},
      out_par_(out_par) {
  std::fprintf(
      file_.get(),
      "# %s initial conditions: hypersurface of constant proper time\n",
      SMASH_VERSION);
  std::fprintf(file_.get(),
               "# tau x y eta mt px py Rap pdg charge "
               "baryon_number strangeness\n");
  std::fprintf(file_.get(),
               "# fm fm fm none GeV GeV GeV none none e "
               "none none\n");
}

ICOutput::~ICOutput() {}

void ICOutput::at_eventstart(const Particles &, const int event_number,
                             const EventInfo &) {
  std::fprintf(file_.get(), "# event %i start\n", event_number);
}

void ICOutput::at_eventend(const Particles &particles, const int event_number,
                           const EventInfo &event) {
  std::fprintf(file_.get(), "# event %i end\n", event_number);

  // If the runtime is too short some particles might not yet have
  // reached the hypersurface. Warning is printed.
  if (particles.size() != 0 && !event.impose_kinematic_cut_for_SMASH_IC) {
    logg[LHyperSurfaceCrossing].warn(
        "End time might be too small for initial conditions output. "
        "Hypersurface has not yet been crossed by ",
        particles.size(), " particle(s).");
  }
}

void ICOutput::at_intermediate_time(const Particles &,
                                    const std::unique_ptr<Clock> &,
                                    const DensityParameters &,
                                    const EventInfo &) {
  // Dummy, but virtual function needs to be declared.
}

void ICOutput::at_interaction(const Action &action, const double) {
  assert(action.get_type() == ProcessType::HyperSurfaceCrossing);
  assert(action.incoming_particles().size() == 1);

  ParticleData particle = action.incoming_particles()[0];

  // transverse mass
  const double m_trans =
      std::sqrt(particle.type().mass() * particle.type().mass() +
                particle.momentum()[1] * particle.momentum()[1] +
                particle.momentum()[2] * particle.momentum()[2]);
  // momentum space rapidity
  const double rapidity =
      0.5 * std::log((particle.momentum()[0] + particle.momentum()[3]) /
                     (particle.momentum()[0] - particle.momentum()[3]));

  // Determine if particle is spectator:
  // Fulfilled if particle is initial nucleon, aka has no prior interactions
  bool is_spectator = particle.get_history().collisions_per_particle == 0;

  // write particle data excluding spectators
  if (!is_spectator) {
    std::fprintf(file_.get(), "%g %g %g %g %g %g %g %g %s %i %i %i \n",
                 particle.position().tau(), particle.position()[1],
                 particle.position()[2], particle.position().eta(), m_trans,
                 particle.momentum()[1], particle.momentum()[2], rapidity,
                 particle.pdgcode().string().c_str(), particle.type().charge(),
                 particle.type().baryon_number(),
                 particle.type().strangeness());
  }

  if (IC_proper_time_ < 0.0) {
    // First particle that is removed, overwrite negative default
    IC_proper_time_ = particle.position().tau();
  } else {
    // Verify that all other particles have the same proper time
    const double next_proper_time = particle.position().tau();
    if (!((next_proper_time - IC_proper_time_) < really_small))
      throw std::runtime_error(
          "Hypersurface proper time changed during evolution.");
  }
}

}  // namespace smash
