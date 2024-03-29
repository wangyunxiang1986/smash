/*
 *
 *    Copyright (c) 2016-2020,2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "smash/hadgas_eos.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "gsl/gsl_sf_bessel.h"

#include "smash/constants.h"
#include "smash/integrate.h"
#include "smash/interpolation.h"
#include "smash/logging.h"
#include "smash/random.h"

namespace smash {
static constexpr int LResonances = LogArea::Resonances::id;

EosTable::EosTable(double de, double dnb, double dq, size_t n_e, size_t n_nb,
                   size_t n_q)
    : de_(de), dnb_(dnb), dq_(dq), n_e_(n_e), n_nb_(n_nb), n_q_(n_q) {
  table_.resize(n_e_ * n_nb_ * n_q_);
}

void EosTable::compile_table(HadronGasEos &eos,
                             const std::string &eos_savefile_name) {
  bool table_read_success = false, table_consistency = true;
  if (std::filesystem::exists(eos_savefile_name)) {
    std::cout << "Reading table from file " << eos_savefile_name << std::endl;
    std::ifstream file;
    file.open(eos_savefile_name, std::ios::in);
    file >> de_ >> dnb_ >> dq_;
    file >> n_e_ >> n_nb_ >> n_q_;
    table_.resize(n_e_ * n_nb_ * n_q_);
    for (size_t ie = 0; ie < n_e_; ie++) {
      for (size_t inb = 0; inb < n_nb_; inb++) {
        for (size_t iq = 0; iq < n_q_; iq++) {
          double p, T, mub, mus, muq;
          file >> p >> T >> mub >> mus >> muq;
          table_[index(ie, inb, iq)] = {p, T, mub, mus, muq};
        }
      }
    }
    table_read_success = true;
    std::cout << "Table consumed successfully." << std::endl;
  }

  if (table_read_success) {
    // Check if the saved table is consistent with the current particle table
    std::cout << "Checking consistency of the table... " << std::endl;
    constexpr size_t number_of_steps = 50;
    const size_t ie_step = 1 + n_e_ / number_of_steps;
    const size_t inb_step = 1 + n_nb_ / number_of_steps;
    const size_t iq_step = 1 + n_q_ / number_of_steps;
    for (size_t ie = 0; ie < n_e_; ie += ie_step) {
      for (size_t inb = 0; inb < n_nb_; inb += inb_step) {
        for (size_t iq = 0; iq < n_q_; iq += iq_step) {
          const table_element x = table_[index(ie, inb, iq)];
          const bool w = eos.account_for_resonance_widths();
          const double e_comp = eos.energy_density(x.T, x.mub, x.mus, x.muq);
          const double nb_comp =
              eos.net_baryon_density(x.T, x.mub, x.mus, x.muq, w);
          const double ns_comp =
              eos.net_strange_density(x.T, x.mub, x.mus, x.muq, w);
          const double p_comp = eos.pressure(x.T, x.mub, x.mus, x.muq, w);
          const double nq_comp =
              eos.net_charge_density(x.T, x.mub, x.mus, x.muq, w);
          // Precision is just 10^-3, this is precision of saved data in the
          // file
          const double eps = 1.e-3;
          // Only check the physical region, hence T > 0 condition
          if ((std::abs(de_ * ie - e_comp) > eps ||
               std::abs(dnb_ * inb - nb_comp) > eps ||
               std::abs(ns_comp) > eps || std::abs(x.p - p_comp) > eps ||
               std::abs(dq_ * iq - nq_comp) > eps) &&
              (x.T > 0.0)) {
            std::cout << "discrepancy: " << de_ * ie << " = " << e_comp << ", "
                      << dnb_ * inb << " = " << nb_comp << ", " << x.p << " = "
                      << p_comp << ", 0 = " << ns_comp << ", " << dq_ * iq
                      << " = " << nq_comp << std::endl;
            table_consistency = false;
            goto finish_consistency_check;
          }
        }
      }
    }
  }
finish_consistency_check:

  if (!table_read_success || !table_consistency) {
    std::cout << "Compiling an EoS table..." << std::endl;
    const double ns = 0.0;
    for (size_t ie = 0; ie < n_e_; ie++) {
      std::cout << ie << "/" << n_e_ << "\r" << std::flush;
      const double e = de_ * ie;
      for (size_t inb = 0; inb < n_nb_; inb++) {
        const double nb = dnb_ * inb;
        for (size_t iq = 0; iq < n_q_; iq++) {
          const double q = dq_ * iq;
          // It is physically impossible to have energy density > nucleon
          // mass*nb, therefore eqns have no solutions.
          if (nb >= e || q >= e) {
            table_[index(ie, inb, iq)] = {0.0, 0.0, 0.0, 0.0, 0.0};
            continue;
          }
          // Take extrapolated (T, mub, mus, muq) as initial approximation
          std::array<double, 4> init_approx;
          if (inb >= 2) {
            const table_element y = table_[index(ie, inb - 2, iq)];
            const table_element x = table_[index(ie, inb - 1, iq)];
            init_approx = {2.0 * x.T - y.T, 2.0 * x.mub - y.mub,
                           2.0 * x.mus - y.mus, 2.0 * x.muq - y.muq};
          } else if (iq >= 2) {
            const table_element y = table_[index(ie, inb, iq - 2)];
            const table_element x = table_[index(ie, inb, iq - 1)];
            init_approx = {2.0 * x.T - y.T, 2.0 * x.mub - y.mub,
                           2.0 * x.mus - y.mus, 2.0 * x.muq - y.muq};
          } else {
            init_approx = eos.solve_eos_initial_approximation(e, nb, q);
          }
          const std::array<double, 4> res =
              eos.solve_eos(e, nb, ns, q, init_approx);
          const double T = res[0];
          const double mub = res[1];
          const double mus = res[2];
          const double muq = res[3];
          const bool w = eos.account_for_resonance_widths();
          table_[index(ie, inb, iq)] = {eos.pressure(T, mub, mus, muq, w), T,
                                        mub, mus, muq};
        }
      }
    }
    // Save table to file
    std::cout << "Saving table to file " << eos_savefile_name << std::endl;
    std::ofstream file;
    file.open(eos_savefile_name, std::ios::out);
    file << de_ << " " << dnb_ << " " << dq_ << std::endl;
    file << n_e_ << " " << n_nb_ << " " << n_q_ << std::endl;
    file << std::setprecision(7);
    file << std::fixed;
    for (size_t ie = 0; ie < n_e_; ie++) {
      for (size_t inb = 0; inb < n_nb_; inb++) {
        for (size_t iq = 0; iq < n_q_; iq++) {
          const EosTable::table_element x = table_[index(ie, inb, iq)];
          file << x.p << " " << x.T << " " << x.mub << " " << x.mus << " "
               << x.muq << std::endl;
        }
      }
    }
  }
}

void EosTable::get(EosTable::table_element &res, double e, double nb,
                   double q) const {
  const size_t ie = static_cast<size_t>(std::floor(e / de_));
  const size_t inb = static_cast<size_t>(std::floor(nb / dnb_));
  const size_t iq = static_cast<size_t>(std::floor(q / dq_));

  if (ie >= n_e_ - 1 || inb >= n_nb_ - 1 || iq >= n_q_ - 1) {
    res = {-1.0, -1.0, -1.0, -1.0, -1.0};
  } else {
    // 1st order interpolation
    const double ae = e / de_ - ie;
    const double an = nb / dnb_ - inb;
    const double aq = q / dq_ - iq;
    const EosTable::table_element s1 = table_[index(ie, inb, iq)];
    const EosTable::table_element s2 = table_[index(ie + 1, inb, iq)];
    const EosTable::table_element s3 = table_[index(ie, inb + 1, iq)];
    const EosTable::table_element s4 = table_[index(ie + 1, inb + 1, iq)];
    const EosTable::table_element s5 = table_[index(ie, inb, iq + 1)];
    const EosTable::table_element s6 = table_[index(ie + 1, inb, iq + 1)];
    const EosTable::table_element s7 = table_[index(ie, inb + 1, iq + 1)];
    const EosTable::table_element s8 = table_[index(ie + 1, inb + 1, iq + 1)];

    res.p = interpolate_trilinear(ae, an, aq, s1.p, s2.p, s3.p, s4.p, s5.p,
                                  s6.p, s7.p, s8.p);
    res.T = interpolate_trilinear(ae, an, aq, s1.T, s2.T, s3.T, s4.T, s5.T,
                                  s6.T, s7.T, s8.T);
    res.mub = interpolate_trilinear(ae, an, aq, s1.mub, s2.mub, s3.mub, s4.mub,
                                    s5.mub, s6.mub, s7.mub, s8.mub);
    res.mus = interpolate_trilinear(ae, an, aq, s1.mus, s2.mus, s3.mus, s4.mus,
                                    s5.mus, s6.mus, s7.mus, s8.mus);
    res.muq = interpolate_trilinear(ae, an, aq, s1.muq, s2.muq, s3.muq, s4.muq,
                                    s5.muq, s6.muq, s7.muq, s8.muq);
  }
}

HadronGasEos::HadronGasEos(bool tabulate, bool account_for_width)
    : x_(gsl_vector_alloc(n_equations_)),
      tabulate_(tabulate),
      account_for_resonance_widths_(account_for_width) {
  const gsl_multiroot_fsolver_type *solver_type;
  solver_type = gsl_multiroot_fsolver_hybrid;
  solver_ = gsl_multiroot_fsolver_alloc(solver_type, n_equations_);
  if (tabulate_ && account_for_resonance_widths_) {
    logg[LResonances].error(
        "Compilation of hadron gas EoS table requested with"
        " account of resonance spectral functions. This is not "
        "advised, as it will likely take a few days to finish."
        " Besides, the effect of resonance widths is currently not "
        "implemented for energy density computation, so the computed"
        " table will be inconsistent anyways.");
  }
  if (tabulate_) {
    eos_table_.compile_table(*this);
  }
}

HadronGasEos::~HadronGasEos() {
  gsl_multiroot_fsolver_free(solver_);
  gsl_vector_free(x_);
}

double HadronGasEos::scaled_partial_density_auxiliary(double m_over_T,
                                                      double mu_over_T) {
  double x = mu_over_T - m_over_T;
  if (x < -500.0) {
    return 0.0;
  }
  x = std::exp(x);
  // In the case of small masses: K_n(z) -> (n-1)!/2 *(2/z)^n, z -> 0,
  // z*z*K_2(z) -> 2
  return (m_over_T < really_small)
             ? 2.0 * x
             : m_over_T * m_over_T * x * gsl_sf_bessel_Kn_scaled(2, m_over_T);
}

double HadronGasEos::scaled_partial_density(const ParticleType &ptype,
                                            double beta, double mub, double mus,
                                            double muq,
                                            bool account_for_width) {
  const double m_over_T = ptype.mass() * beta;
  double mu_over_T = beta * (ptype.baryon_number() * mub +
                             ptype.strangeness() * mus + ptype.charge() * muq);
  const double g = ptype.spin() + 1;
  if (ptype.is_stable() || !account_for_width) {
    return g * scaled_partial_density_auxiliary(m_over_T, mu_over_T);
  } else {
    // Integral \int_{threshold}^{\infty} A(m) N_{thermal}(m) dm,
    // where A(m) is the spectral function of the resonance.
    const double m0 = ptype.mass();
    const double w0 = ptype.width_at_pole();
    const double mth = ptype.min_mass_spectral();
    const double u_min = std::atan(2.0 * (mth - m0) / w0);
    const double u_max = 0.5 * M_PI;
    Integrator integrate;
    const double result =
        g * integrate(u_min, u_max, [&](double u) {
          // One of many possible variable substitutions. Not clear if it has
          // any advantages, except transforming (m_th, inf) to finite interval.
          const double tanu = std::tan(u);
          const double m = m0 + 0.5 * w0 * tanu;
          const double jacobian = 0.5 * w0 * (1.0 + tanu * tanu);
          return ptype.spectral_function(m) * jacobian *
                 scaled_partial_density_auxiliary(m * beta, mu_over_T);
        });
    return result;
  }
}

double HadronGasEos::partial_density(const ParticleType &ptype, double T,
                                     double mub, double mus, double muq,
                                     bool account_for_width) {
  if (T < really_small) {
    return 0.0;
  }
  return prefactor_ * T * T * T *
         scaled_partial_density(ptype, 1.0 / T, mub, mus, muq,
                                account_for_width);
}

double HadronGasEos::energy_density(double T, double mub, double mus,
                                    double muq) {
  if (T < really_small) {
    return 0.0;
  }
  const double beta = 1.0 / T;
  double e = 0.0;
  for (const ParticleType &ptype : ParticleType::list_all()) {
    if (!is_eos_particle(ptype)) {
      continue;
    }
    const double z = ptype.mass() * beta;
    double x = beta * (mub * ptype.baryon_number() + mus * ptype.strangeness() +
                       muq * ptype.charge() - ptype.mass());
    if (x < -500.0) {
      return 0.0;
    }
    x = std::exp(x);
    const size_t g = ptype.spin() + 1;
    // Small mass case, z*z*K_2(z) -> 2, z*z*z*K_1(z) -> 0 at z->0
    e += (z < really_small) ? 3.0 * g * x
                            : z * z * g * x *
                                  (3.0 * gsl_sf_bessel_Kn_scaled(2, z) +
                                   z * gsl_sf_bessel_K1_scaled(z));
  }
  e *= prefactor_ * T * T * T * T;
  return e;
}

double HadronGasEos::density(double T, double mub, double mus, double muq,
                             bool account_for_width) {
  if (T < really_small) {
    return 0.0;
  }
  const double beta = 1.0 / T;
  double rho = 0.0;
  for (const ParticleType &ptype : ParticleType::list_all()) {
    if (!is_eos_particle(ptype)) {
      continue;
    }
    rho +=
        scaled_partial_density(ptype, beta, mub, mus, muq, account_for_width);
  }
  rho *= prefactor_ * T * T * T;
  return rho;
}

double HadronGasEos::net_baryon_density(double T, double mub, double mus,
                                        double muq, bool account_for_width) {
  if (T < really_small) {
    return 0.0;
  }
  const double beta = 1.0 / T;
  double rho = 0.0;
  for (const ParticleType &ptype : ParticleType::list_all()) {
    if (!ptype.is_baryon() || !is_eos_particle(ptype)) {
      continue;
    }
    rho +=
        scaled_partial_density(ptype, beta, mub, mus, muq, account_for_width) *
        ptype.baryon_number();
  }
  rho *= prefactor_ * T * T * T;
  return rho;
}

double HadronGasEos::net_strange_density(double T, double mub, double mus,
                                         double muq, bool account_for_width) {
  if (T < really_small) {
    return 0.0;
  }
  const double beta = 1.0 / T;
  double rho = 0.0;
  for (const ParticleType &ptype : ParticleType::list_all()) {
    if (ptype.strangeness() == 0 || !is_eos_particle(ptype)) {
      continue;
    }
    rho +=
        scaled_partial_density(ptype, beta, mub, mus, muq, account_for_width) *
        ptype.strangeness();
  }
  rho *= prefactor_ * T * T * T;
  return rho;
}

double HadronGasEos::net_charge_density(double T, double mub, double mus,
                                        double muq, bool account_for_width) {
  if (T < really_small) {
    return 0.0;
  }
  const double beta = 1.0 / T;
  double rho = 0.0;
  for (const ParticleType &ptype : ParticleType::list_all()) {
    if (ptype.charge() == 0 || !is_eos_particle(ptype)) {
      continue;
    }
    rho +=
        scaled_partial_density(ptype, beta, mub, mus, muq, account_for_width) *
        ptype.charge();
  }
  rho *= prefactor_ * T * T * T;
  return rho;
}

double HadronGasEos::sample_mass_thermal(const ParticleType &ptype,
                                         double beta) {
  if (ptype.is_stable()) {
    return ptype.mass();
  }
  // Sampling mass m from A(m) x^2 BesselK_2(x), where x = beta m.
  // Strategy employs the idea of importance sampling:
  // -- Sample mass from the simple Breit-Wigner first, then
  //    reject by the ratio.
  // -- Note that f(x) = x^2 BesselK_2(x) monotonously decreases
  //    and has maximum at x = xmin. That is why instead of f(x)
  //    the ratio f(x)/f(xmin) is used.

  const double max_mass = 5.0;  // GeV
  double m, q;
  {
    // Allow underflows in exponentials
    DisableFloatTraps guard(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);
    const double w0 = ptype.width_at_pole();
    const double mth = ptype.min_mass_spectral();
    const double m0 = ptype.mass();
    double max_ratio =
        m0 * m0 * std::exp(-beta * m0) * gsl_sf_bessel_Kn_scaled(2, m0 * beta) *
        ptype.spectral_function(m0) / ptype.spectral_function_simple(m0);
    // Heuristic adaptive maximum search to find max_ratio
    constexpr int npoints = 31;
    double m_lower = mth, m_upper = max_mass, m_where_max = m0;

    for (size_t n_iterations = 0; n_iterations < 2; n_iterations++) {
      const double dm = (m_upper - m_lower) / npoints;
      for (size_t i = 1; i < npoints; i++) {
        m = m_lower + dm * i;
        const double thermal_factor =
            m * m * std::exp(-beta * m) * gsl_sf_bessel_Kn_scaled(2, m * beta);
        q = ptype.spectral_function(m) * thermal_factor /
            ptype.spectral_function_simple(m);
        if (q > max_ratio) {
          max_ratio = q;
          m_where_max = m;
        }
      }
      m_lower = m_where_max - (m_where_max - m_lower) * 0.1;
      m_upper = m_where_max + (m_upper - m_where_max) * 0.1;
    }
    // Safety factor
    max_ratio *= 1.5;

    do {
      // sample mass from A(m)
      do {
        m = random::cauchy(m0, 0.5 * w0, mth, max_mass);
        const double thermal_factor =
            m * m * std::exp(-beta * m) * gsl_sf_bessel_Kn_scaled(2, m * beta);
        q = ptype.spectral_function(m) * thermal_factor /
            ptype.spectral_function_simple(m);
      } while (q < random::uniform(0., max_ratio));
      if (q > max_ratio) {
        logg[LResonances].warn(
            ptype.name(), " - maximum increased in",
            " sample_mass_thermal from ", max_ratio, " to ", q, ", mass = ", m,
            " previously assumed maximum at m = ", m_where_max);
        max_ratio = q;
        m_where_max = m;
      } else {
        break;
      }
    } while (true);
  }
  return m;
}

double HadronGasEos::mus_net_strangeness0(double T, double mub, double muq) {
  // Binary search
  double mus_u = mub + T;
  double mus_l = 0.0;
  double mus, rhos;
  size_t iteration = 0;
  // 50 iterations should give precision 2^-50 ~ 10^-15
  const size_t max_iteration = 50;
  do {
    mus = 0.5 * (mus_u + mus_l);
    rhos = net_strange_density(T, mub, mus, muq);
    if (rhos > 0.0) {
      mus_u = mus;
    } else {
      mus_l = mus;
    }
    iteration++;
  } while (std::abs(rhos) > tolerance_ && iteration < max_iteration);
  if (iteration == max_iteration) {
    throw std::runtime_error("Solving rho_s = 0: too many iterations.");
  }
  return mus;
}

int HadronGasEos::set_eos_solver_equations(const gsl_vector *x, void *params,
                                           gsl_vector *f) {
  double e = reinterpret_cast<struct rparams *>(params)->e;
  double nb = reinterpret_cast<struct rparams *>(params)->nb;
  double ns = reinterpret_cast<struct rparams *>(params)->ns;
  double nq = reinterpret_cast<struct rparams *>(params)->nq;
  bool w = reinterpret_cast<struct rparams *>(params)->account_for_width;

  const double T = gsl_vector_get(x, 0);
  const double mub = gsl_vector_get(x, 1);
  const double mus = gsl_vector_get(x, 2);
  const double muq = gsl_vector_get(x, 3);

  gsl_vector_set(f, 0, energy_density(T, mub, mus, muq) - e);
  gsl_vector_set(f, 1, net_baryon_density(T, mub, mus, muq, w) - nb);
  gsl_vector_set(f, 2, net_strange_density(T, mub, mus, muq, w) - ns);
  gsl_vector_set(f, 3, net_charge_density(T, mub, mus, muq, w) - nq);

  return GSL_SUCCESS;
}

double HadronGasEos::e_equation(double T, void *params) {
  const double edens = reinterpret_cast<struct eparams *>(params)->edens;
  return edens - energy_density(T, 0.0, 0.0, 0.0);
}

std::array<double, 4> HadronGasEos::solve_eos_initial_approximation(double e,
                                                                    double nb,
                                                                    double nq) {
  assert(e >= 0.0);
  // 1. Get temperature from energy density assuming zero chemical potentials
  int degeneracies_sum = 0.0;
  for (const ParticleType &ptype : ParticleType::list_all()) {
    if (is_eos_particle(ptype)) {
      degeneracies_sum += ptype.spin() + 1;
    }
  }
  // Temperature in case of massless gas. For massive it should be larger.
  const double T_min = std::pow(e / prefactor_ / 6 / degeneracies_sum, 1. / 4.);
  // Simply assume that the temperature is not higher than 2 GeV.
  const double T_max = 2.0;

  struct eparams parameters = {e};
  gsl_function F = {&e_equation, &parameters};
  const gsl_root_fsolver_type *T = gsl_root_fsolver_brent;
  gsl_root_fsolver *e_solver;
  e_solver = gsl_root_fsolver_alloc(T);
  gsl_root_fsolver_set(e_solver, &F, T_min, T_max);

  int iter = 0, status, max_iter = 100;
  double T_init = 0.0;

  do {
    iter++;
    status = gsl_root_fsolver_iterate(e_solver);
    if (status != GSL_SUCCESS) {
      break;
    }
    T_init = gsl_root_fsolver_root(e_solver);
    double x_lo = gsl_root_fsolver_x_lower(e_solver);
    double x_hi = gsl_root_fsolver_x_upper(e_solver);
    status = gsl_root_test_interval(x_lo, x_hi, 0.0, 0.001);
  } while (status == GSL_CONTINUE && iter < max_iter);

  if (status != GSL_SUCCESS) {
    std::stringstream err_msg;
    err_msg << "Solver of equation for temperature with e = " << e
            << " failed to converge. Maybe Tmax = " << T_max << " is too small?"
            << std::endl;
    throw std::runtime_error(gsl_strerror(status) + err_msg.str());
  }

  gsl_root_fsolver_free(e_solver);

  // 2. Get the baryon chemical potential for muS = muQ = 0 with previously
  //    obtained T
  double n_only_baryons = 0.0;
  for (const ParticleType &ptype : ParticleType::list_all()) {
    if (is_eos_particle(ptype) && ptype.baryon_number() == 1) {
      n_only_baryons +=
          scaled_partial_density(ptype, 1.0 / T_init, 0.0, 0.0, 0.0);
    }
  }
  const double nb_scaled = nb / prefactor_ / (T_init * T_init * T_init);
  double mub_init = T_init * std::asinh(nb_scaled / n_only_baryons / 2.0);

  // 3. Get the charge chemical potential assuming muB = muS = 0 with previously
  //    obtained T
  double n_only_charge_1_particles = 0.0;
  for (const ParticleType &ptype : ParticleType::list_all()) {
    if (is_eos_particle(ptype) && ptype.charge() == 1) {
      n_only_charge_1_particles +=
          scaled_partial_density(ptype, 1.0 / T_init, 0.0, 0.0, 0.0);
    }
  }
  const double q_scaled = nq / prefactor_ / (T_init * T_init * T_init);
  double muq_init =
      T_init * std::asinh(q_scaled / n_only_charge_1_particles / 2.0);

  // 4. Get the strange chemical potential, where mus = 0 is typically a good
  //    initial approximation
  std::array<double, 4> initial_approximation = {T_init, mub_init, 0.0,
                                                 muq_init};
  return initial_approximation;
}

std::array<double, 4> HadronGasEos::solve_eos(
    double e, double nb, double ns, double nq,
    std::array<double, 4> initial_approximation) {
  int residual_status = GSL_SUCCESS;
  size_t iter = 0;

  struct rparams p = {e, nb, ns, nq, account_for_resonance_widths_};
  gsl_multiroot_function f = {&HadronGasEos::set_eos_solver_equations,
                              n_equations_, &p};

  gsl_vector_set(x_, 0, initial_approximation[0]);
  gsl_vector_set(x_, 1, initial_approximation[1]);
  gsl_vector_set(x_, 2, initial_approximation[2]);
  gsl_vector_set(x_, 3, initial_approximation[3]);

  gsl_multiroot_fsolver_set(solver_, &f, x_);
  do {
    iter++;
    const auto iterate_status = gsl_multiroot_fsolver_iterate(solver_);
    // std::cout << print_solver_state(iter);

    // Avoiding too low temperature
    if (gsl_vector_get(solver_->x, 0) < 0.015) {
      return {0.0, 0.0, 0.0, 0.0};
    }

    // check if solver is stuck
    if (iterate_status) {
      break;
    }
    residual_status = gsl_multiroot_test_residual(solver_->f, tolerance_);
  } while (residual_status == GSL_CONTINUE && iter < 1000);

  if (residual_status != GSL_SUCCESS) {
    std::stringstream solver_parameters;
    solver_parameters << "\nSolver run with "
                      << "e = " << e << ", nb = " << nb << ", ns = " << ns
                      << ", nq = " << nq
                      << ", init. approx.: " << initial_approximation[0] << " "
                      << initial_approximation[1] << " "
                      << initial_approximation[2] << " "
                      << initial_approximation[3] << std::endl;
    logg[LResonances].warn(gsl_strerror(residual_status) +
                           solver_parameters.str() + print_solver_state(iter));
  }

  return {gsl_vector_get(solver_->x, 0), gsl_vector_get(solver_->x, 1),
          gsl_vector_get(solver_->x, 2), gsl_vector_get(solver_->x, 3)};
}

std::string HadronGasEos::print_solver_state(size_t iter) const {
  std::stringstream s;
  // clang-format off
  s << "iter = " << iter << ","
    << " x = " << gsl_vector_get(solver_->x, 0) << " "
               << gsl_vector_get(solver_->x, 1) << " "
               << gsl_vector_get(solver_->x, 2) << ", "
               << gsl_vector_get(solver_->x, 3) << ", "
    << "f(x) = " << gsl_vector_get(solver_->f, 0) << " "
                 << gsl_vector_get(solver_->f, 1) << " "
                 << gsl_vector_get(solver_->f, 2) << " "
                 << gsl_vector_get(solver_->f, 3) << std::endl;
  // clang-format on
  return s.str();
}

}  // namespace smash
