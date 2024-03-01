/*
 *
 *    Copyright (c) 2020,2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#ifndef SRC_INCLUDE_SMASH_QUANTUMSAMPLING_H_
#define SRC_INCLUDE_SMASH_QUANTUMSAMPLING_H_

#include <map>

#include "gsl/gsl_multiroots.h"
#include "gsl/gsl_vector.h"

#include "smash/pdgcode.h"
#include "smash/random.h"

namespace smash {

/**
 * This class:
 * - Calculates chemical potentials given density of particle species
 * - Calculates maxima of a Juttner distribution for these chemical potentials
 * - Samples Juttner distribution. This is the main intent of this class,
 *   while previous points are auxiliary calculations for it.
 */

class QuantumSampling {
 public:
  /**
   * Constructor of a QuantumSampling object.
   * \param[in] initial_multiplicities a map of pdg codes of samples particle
   *            species and corresponding multiplicities
   * \param[in] volume volume V in which the particles are sampled [fm^3],
   *            needed to calculate the density of the species
   * \param[in] temperature temperature T of the system [GeV]
   */
  QuantumSampling(const std::map<PdgCode, int>& initial_multiplicities,
                  double volume, double temperature);

  /**
   * Struct object that holds the parameters relevant to finding the momentum
   * for which the maximum of the distribution occurs.
   */
  struct ParametersForMaximumMomentumRootFinder {
    /// mass m of a particle [GeV]
    double mass;
    /// temperature T of the system [GeV]
    double temperature;
    /// effective chemical potential mu^* of the particle species
    double effective_chemical_potential;
    /**
     * quantum statistics of the particles species
     * (+1 for Fermi, -1 for Bose, 0 for Boltzmann)
     */
    double statistics;
  };

  /**
   * Root equation for finding the radial momentum at which the Juttner
   * distribution function has its maximum.
   * \param[in] p radial momentum, i.e., length of the momentum vector [GeV]
   * \param[in] mass (pole) mass m of the particle species [GeV]
   * \param[in] temperature temperature T of the system [GeV]
   * \param[in] effective_chemical_potential effective chemical potential mu of
   *            the system [GeV]
   * \param[in] statistics quantum statistics of the particles species
   *            (+1 for Fermi, -1 for Bose, 0 for Boltzmann)
   * \return the extremum equation for the maximum of the Juttner distribution
   */
  static double p_max_root_equation(double p, double mass, double temperature,
                                    double effective_chemical_potential,
                                    double statistics);

  /**
   * Root equation for finding the radial momentum at which the Juttner
   * distribution function has its maximum, suited for the GSL root finding
   * procedure.
   * \param[in] roots_array an array holding the current best estimate of the
   *            roots of the solved equation
   * \param[in] parameters refers to the parameters as provided in the GSL
   *            root solving procedure
   * \param[in] function refers to the root equation(s) as provided in the GSL
   *            root solving procedure (where it's called "function")
   * \return the root equation suited for GSL root finding procedure
   */
  static int p_max_root_equation_for_GSL(const gsl_vector* roots_array,
                                         void* parameters,
                                         gsl_vector* function);

  /**
   * A GSL utility which allows for printing out the status of the solver
   * during the root finding procedure.
   * \param[in] iter variable keeping track of how many steps in the root
   *            solving procedure have been taken
   * \param[in] solver GSL solver object, which has acces to the current best
   *            estimate of the roots and the corresponding function values
   * \return message about the current state of the solver
   */
  static void print_state_p_max(unsigned int iter,
                                gsl_multiroot_fsolver* solver);

  /**
   * A GSL root solver for finding the radial momentum value at which the
   * maximum of the given Juttner distribution function occurs. For the value of
   * the distribution at the maximum, one shoud use the function
   * maximum_of_the_distribution().
   * \param[in] mass (pole) mass m of the particle species [GeV]
   * \param[in] temperature temperature T of the system [GeV]
   * \param[in] effective_chemical_potential effective chemical potential mu of
   *            the system [GeV]
   * \param[in] statistics quantum statistics of the particles species
   *            (+1 for Fermi, -1 for Bose, 0 for Boltzmann)
   * \param[in] p_max_initial_guess the initial guess for the value of the
   *            solution [GeV]
   * \param[in] solution_precision precision with which the solution is found
   * \param[out] p_max the solution (momentum for which the distribution takes
   *             on the maximum value) stored in an array object [GeV]
   */
  static int find_p_at_maximum_of_the_distribution(
      double mass, double temperature, double effective_chemical_potential,
      double statistics, double p_max_initial_guess, double solution_precision,
      double* p_max);

  /**
   * A convenience wrapper for finding the maximum value of the Juttner
   * distribution, returning the value of the distribution for the momentum
   * at which the maximum occurs, identified by
   * find_p_at_maximum_of_the_distribution().
   * \param[in] mass (pole) mass m of the particle species [GeV]
   * \param[in] temperature temperature T of the system [GeV]
   * \param[in] effective_chemical_potential effective chemical potential mu of
   *            the system [GeV]
   * \param[in] statistics quantum statistics of the particles species
   *            (+1 for Fermi, -1 for Bose, 0 for Boltzmann)
   * \param[in] solution_precision precision with which the solution is found
   */
  static double maximum_of_the_distribution(double mass, double temperature,
                                            double effective_chemical_potential,
                                            double statistics,
                                            double solution_precision);

  /**
   * Sampling radial momenta of given particle species from Boltzmann, Bose, or
   * Fermi distribution. This sampler uses the simplest rejection sampling.
   * \param[in] pdg the pdg code of the sampled particle species
   * return the sampled momentum [GeV]
   */
  double sample(const PdgCode pdg);

 private:
  /// Tabulated effective chemical potentials for every particle species
  std::map<PdgCode, double> effective_chemical_potentials_;
  /// Tabulated distribution function maxima for every particle species
  std::map<PdgCode, double> distribution_function_maximums_;
  /// Volume [fm^3] in which particles sre sampled
  const double volume_;
  /// Temperature [GeV]
  const double temperature_;
};

}  // namespace smash

#endif  // SRC_INCLUDE_SMASH_QUANTUMSAMPLING_H_
