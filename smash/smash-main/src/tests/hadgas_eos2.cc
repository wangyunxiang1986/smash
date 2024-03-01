/*
 *
 *    Copyright (c) 2018-2020,2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "vir/test.h"  // This include has to be first

#include "gsl/gsl_sf_bessel.h"

#include "histogram.h"
#include "setup.h"
#include "smash/constants.h"
#include "smash/hadgas_eos.h"

using namespace smash;

TEST(create_particles_table) {
  Test::create_actual_particletypes();
  Test::create_actual_decaymodes();
}

TEST(partial_density) {
  const ParticleType& pip = ParticleType::find(0x211);
  const ParticleType& rhop = ParticleType::find(0x213);
  const double T = 0.15, mub = 0.0, mus = 0.0, muq = 0.0;
  constexpr bool account_for_spectral_function = true;
  const double dpip = HadronGasEos::partial_density(pip, T, mub, mus, muq);
  const double drhop = HadronGasEos::partial_density(
      rhop, T, mub, mus, muq, account_for_spectral_function);
  COMPARE_ABSOLUTE_ERROR(dpip, 0.0372145, 1.e-6);
  COMPARE(rhop.mass(), 0.776);
  COMPARE(rhop.width_at_pole(), 0.149);
  COMPARE_ABSOLUTE_ERROR(rhop.spectral_function(rhop.mass()), 4.1841, 1.e-4);
  COMPARE_ABSOLUTE_ERROR(drhop, 0.00682181, 1.e-5);
}

TEST(sample_mass_thermal) {
  const ParticleType& rhop = ParticleType::find(0x213);
  const double T = 0.15;

  const double dm = 0.01;
  Histogram1d hist(dm);
  constexpr int N_TEST = 1E5;  // number of samples
  hist.populate(N_TEST, [&]() {
    return HadronGasEos::sample_mass_thermal(rhop, 1.0 / T);
  });
  hist.test(
      [&](double m) {
        return rhop.spectral_function(m) * m * m * gsl_sf_bessel_Kn(2, m / T) *
               HadronGasEos::partial_density(rhop, T, 0.0, 0.0, 0.0);
      },
      "mass_sampling.dat");
}
