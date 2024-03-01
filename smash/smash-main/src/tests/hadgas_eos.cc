/*
 *
 *    Copyright (c) 2016-2020,2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "vir/test.h"  // This include has to be first

#include "smash/hadgas_eos.h"

#include "setup.h"
#include "smash/constants.h"

using namespace smash;

TEST(create_particles_table) {
  Test::create_actual_particletypes();
  Test::create_actual_decaymodes();
}

TEST(td_simple_gas) {
  const double T = 0.1;
  const double mub = 0.8;
  const double mus = 0.1;
  const double muq = 0.05;
  COMPARE_ABSOLUTE_ERROR(HadronGasEos::net_baryon_density(T, mub, mus, muq),
                         0.579156863, 1.e-6);
  COMPARE_ABSOLUTE_ERROR(HadronGasEos::net_strange_density(T, mub, mus, muq),
                         -0.03686360221, 1.e-6);
  COMPARE_ABSOLUTE_ERROR(HadronGasEos::net_charge_density(T, mub, mus, muq),
                         0.4201279754, 1.e-6);
  COMPARE_ABSOLUTE_ERROR(HadronGasEos::density(T, mub, mus, muq), 0.6190973942,
                         1.e-6);
  COMPARE_ABSOLUTE_ERROR(HadronGasEos::pressure(T, mub, mus, muq),
                         0.06190973942, 1.e-6);
  COMPARE_ABSOLUTE_ERROR(HadronGasEos::energy_density(T, mub, mus, muq),
                         0.7252341309, 1.e-6);
}

TEST(mu_zero_net_strangeness) {
  const double mub = 0.6;
  const double muq = 0.1;
  const double T = 0.05;
  const double mus = HadronGasEos::mus_net_strangeness0(T, mub, muq);
  const double ns = HadronGasEos::net_strange_density(T, mub, mus, muq);
  VERIFY(std::abs(ns) < 1.e-4) << ns << ", mus = " << mus;
}

TEST(solve_EoS_substitute) {
  const double mub = 0.2;
  const double muq = 0.1;
  const double mus = 0.0;
  const double T = 0.30;
  const double e = HadronGasEos::energy_density(T, mub, mus, muq);
  const double nb = HadronGasEos::net_baryon_density(T, mub, mus, muq);
  const double ns = HadronGasEos::net_strange_density(T, mub, mus, muq);
  const double nq = HadronGasEos::net_charge_density(T, mub, mus, muq);
  HadronGasEos eos = HadronGasEos(false, false);
  const std::array<double, 4> sol = eos.solve_eos(e, nb, ns, nq);
  COMPARE_ABSOLUTE_ERROR(sol[0], T, 1.e-4);
  COMPARE_ABSOLUTE_ERROR(sol[1], mub, 1.e-4);
  COMPARE_ABSOLUTE_ERROR(sol[2], mus, 1.e-4);
  COMPARE_ABSOLUTE_ERROR(sol[3], muq, 1.e-4);
}

TEST(EoS_table) {
  // make a small table of EoS
  HadronGasEos eos = HadronGasEos(false, false);
  EosTable table = EosTable(0.1, 0.05, 0.05, 5, 5, 5);
  table.compile_table(eos, "small_test_table_eos.dat");
  EosTable::table_element x;
  const double my_e = 0.39, my_nb = 0.09, my_nq = 0.06;
  table.get(x, my_e, my_nb, my_nq);
  // check if tabulated values are the right solutions
  COMPARE_ABSOLUTE_ERROR(HadronGasEos::energy_density(x.T, x.mub, x.mus, x.muq),
                         my_e, 1.e-2);
  COMPARE_ABSOLUTE_ERROR(
      HadronGasEos::net_baryon_density(x.T, x.mub, x.mus, x.muq), my_nb, 1.e-2);
  COMPARE_ABSOLUTE_ERROR(
      HadronGasEos::net_charge_density(x.T, x.mub, x.mus, x.muq), my_nq, 1.e-2);
  remove("small_test_table_eos.dat");
}

/*
TEST(make_test_table) {
  // To switch on these tests, comment out the previous ones.
  // Otherwise the particle table confilct emerges.
  Test::create_actual_particletypes();
  const double ns = 0.0;
  const double nb = 0.3;
  HadronGasEos eos = HadronGasEos(false, false);
  for (int ie = 0; ie < 1000; ie++) {
    const double e = nb + 0.001 + 0.001 * ie;
    const std::array<double, 3> sol = eos.solve_eos(e, nb, ns);
    std::cout << e << " " << HadronGasEos::pressure(sol[0], sol[1], sol[2]) <<
std::endl;
  }
}

TEST(make_test_table2) {
  const double mub = 0.0;
  const double mus = 0.0;
  for (int it = 0; it < 1000; it++) {
    const double T = 0.070 + 0.001 * it;
    const double e  = HadronGasEos::energy_density(T, mub, mus);
    const double p  = HadronGasEos::pressure(T, mub, mus);
    std::cout << T << " " << (e-3*p)/(T*T*T*T) << std::endl;
  }
}*/
