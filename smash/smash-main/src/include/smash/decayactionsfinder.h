/*
 *
 *    Copyright (c) 2014-2020,2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#ifndef SRC_INCLUDE_SMASH_DECAYACTIONSFINDER_H_
#define SRC_INCLUDE_SMASH_DECAYACTIONSFINDER_H_

#include <vector>

#include "actionfinderfactory.h"

namespace smash {

/**
 * \ingroup action
 * A simple decay finder:
 * Just loops through all particles and checks if they can decay during the next
 * timestep.
 */
class DecayActionsFinder : public ActionFinderInterface {
 public:
  /**
   * Initialize the finder
   *
   * \param[in] res_lifetime_factor The multiplicative factor to be applied to
   *                                resonance lifetimes; default is 1
   * \param[in] do_weak_decays whether to perform weak decays at the end
   */
  explicit DecayActionsFinder(double res_lifetime_factor, bool do_weak_decays)
      : res_lifetime_factor_(res_lifetime_factor),
        do_final_weak_decays_(do_weak_decays) {}

  /**
   * Check the whole particle list for decays.
   *
   * \param[in] search_list All particles in grid cell.
   * \param[in] dt Size of timestep [fm]
   * \return List with the found (Decay)Action objects.
   */
  ActionList find_actions_in_cell(
      const ParticleList &search_list, double dt, const double,
      const std::vector<FourVector> &) const override;

  /// Ignore the neighbor searches for decays
  ActionList find_actions_with_neighbors(
      const ParticleList &, const ParticleList &, double,
      const std::vector<FourVector> &) const override {
    return {};
  }

  /// Ignore the surrounding searches for decays
  ActionList find_actions_with_surrounding_particles(
      const ParticleList &, const Particles &, double,
      const std::vector<FourVector> &) const override {
    return {};
  }

  /**
   * Force all resonances to decay at the end of the simulation.
   *
   * \param[in] search_list All particles at the end of simulation.
   * \param[in] only_res optional parameter that requests that only actions
   *                     regarding resonances are considered (disregarding
   *                     stable particles)
   * \return List with the found (Decay)Action objects.
   */
  ActionList find_final_actions(const Particles &search_list,
                                bool only_res = false) const override;

  /// Multiplicative factor to be applied to resonance lifetimes
  const double res_lifetime_factor_ = 1.;

  /**
   * Do weak decays at the end? Weak here means all non-strong decays,
   * so electro-magnetic decays are done as well.
   */
  const bool do_final_weak_decays_;
};

}  // namespace smash

#endif  // SRC_INCLUDE_SMASH_DECAYACTIONSFINDER_H_
