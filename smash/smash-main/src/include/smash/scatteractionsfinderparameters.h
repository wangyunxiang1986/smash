/*
 *    Copyright (c) 2022-2023
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 */
#ifndef SRC_INCLUDE_SMASH_SCATTERACTIONSFINDERPARAMETERS_H_
#define SRC_INCLUDE_SMASH_SCATTERACTIONSFINDERPARAMETERS_H_

#include <memory>
#include <set>
#include <utility>

#include "input_keys.h"

namespace smash {

/// Constants related to transition between low and high collision energies.
struct StringTransitionParameters {
  /// Transition range in N\f$\pi \f$ collisions
  const std::pair<double, double> sqrts_range_Npi =
      InputKeys::collTerm_stringTrans_rangeNpi.default_value();
  /**
   * Transition range in NN collisions.
   * Tuned to reproduce experimental exclusive cross section data, and at the
   * same produce excitation functions that are as smooth as possible. The
   * default of a 1 GeV range is preserved.
   */
  const std::pair<double, double> sqrts_range_NN =
      InputKeys::collTerm_stringTrans_rangeNN.default_value();
  /**
   * Constant for the lower end of transition region in the case of AQM
   * this is added to the sum of masses
   */
  const double sqrts_add_lower =
      InputKeys::collTerm_stringTrans_lower.default_value();
  /**
   * Constant for the range of transition region, in the case of AQM
   * this is added to the sum of masses + sqrts_add_lower
   */
  const double sqrts_range_width =
      InputKeys::collTerm_stringTrans_range_width.default_value();
  /**
   * Constant offset as to where to turn on the strings and elastic processes
   * for \f$ \pi \pi \f$ reactions (this is an exception because the normal AQM
   * behavior destroys the cross-section at very low \f$\sqrt{s} \f$ and around
   * the \f$ f_2 \f$ peak)
   */
  const double pipi_offset =
      InputKeys::collTerm_stringTrans_pipiOffset.default_value();
  /**
   * Constant offset as to where to shift from 2to2 to string
   * processes (in GeV) in the case of KN reactions
   */
  const double KN_offset =
      InputKeys::collTerm_stringTrans_KNOffset.default_value();
};

/**
 * Helper structure for ScatterActionsFinder.
 *
 * ScatterActionsFinder has one member of this struct, which just collects
 * general parameters, for easier function argument passing.
 */
struct ScatterActionsFinderParameters {
  /// Elastic cross section parameter (in mb).
  const double elastic_parameter;
  /**
   * Elastic collsions between two nucleons with sqrt_s below low_snn_cut_ are
   * excluded.
   */
  const double low_snn_cut;
  /// Factor by which all (partial) cross sections are scaled
  const double scale_xs;
  /**
   * Additional constant contribution (in mb) to the elastic cross sections.
   *
   * Using it will break agreement with experimental data for elastic cross
   * sections that are constrained with data.
   */
  const double additional_el_xs;  // mb
  /// \see input_collision_term_
  const double maximum_cross_section;
  /// Specifies which collision criterion is used
  const CollisionCriterion coll_crit;
  /// Switch for NNbar reactions
  const NNbarTreatment nnbar_treatment;
  /// List of included 2<->2 reactions
  const ReactionsBitSet included_2to2;
  /// List of included multi-particle reactions
  const MultiParticleReactionsBitSet included_multi;
  /// Number of test particles.
  const int testparticles;
  /// Enables resonance production
  const bool two_to_one;
  /// If particles within the same nucleus are allowed to collide for their
  /// first time
  const bool allow_collisions_within_nucleus;
  /// Indicates whether string fragmentation is switched on
  const bool strings_switch;
  /// Switch to control whether to use AQM or not
  const bool use_AQM;
  /**
   * This indicates whether the string fragmentation is swiched on with
   * a probability smoothly increasing with energy. If it's set equal to
   * false, the cross section of the string fragmentation is counted by
   * taking the difference between the parametrized total cross section
   * and the sum of the non-string cross sections.
   */
  const bool strings_with_probability;
  /**
   * Switch to turn off throwing an exception for collision probabilities larger
   * than 1. In larger production runs it is ok, if the probability rarely slips
   * over 1.
   */
  const bool only_warn_for_high_prob;
  /**
   * Constants related to transition between low collision energies - mediated
   * via resonances - and high collision energies - mediated via strings.
   */
  const StringTransitionParameters transition_high_energy;
};

}  // namespace smash

#endif  // SRC_INCLUDE_SMASH_SCATTERACTIONSFINDERPARAMETERS_H_
