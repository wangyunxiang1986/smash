/*
 *    Copyright (c) 2015-2018,2020-2021
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 */

#ifndef SRC_INCLUDE_SMASH_KINEMATICS_H_
#define SRC_INCLUDE_SMASH_KINEMATICS_H_

#include <array>
#include <cassert>
#include <sstream>

#include "constants.h"

namespace smash {

/**
 * \return Velocity in the center of velocities frame of two particles given
 * their Mandelstam s and masses
 * \param[in] s Mandelstam s of the collision [GeV^2]
 * \param[in] ma Mass of the first particle [GeV]
 * \param[in] mb Mass of the second particle [GeV]
 */
inline double center_of_velocity_v(double s, double ma, double mb) {
  const double m_sum = ma + mb;
  const double m_dif = ma - mb;
  return std::sqrt((s - m_sum * m_sum) / (s - m_dif * m_dif));
}

/**
 * \return Velocity of projectile in the fixed target frame given
 * the Mandelstam s of projectile and target and their masses
 * \param[in] s Mandelstam s of the collision [GeV^2]
 * \param[in] ma Mass of the projectile [GeV]
 * \param[in] mb Mass of the target [GeV]
 */
inline double fixed_target_projectile_v(double s, double ma, double mb) {
  const double inv_gamma = 2 * ma * mb / (s - ma * ma - mb * mb);
  return std::sqrt(1.0 - inv_gamma * inv_gamma);
}

/**
 * \return The squared center-of-mass momentum of two particles,
 * given s and their masses. [GeV^2]
 * \param[in] s Mandelstam s of the process [GeV^2].
 * \param[in] mass_a Mass of first particle [GeV].
 * \param[in] mass_b Mass of second particle [GeV].
 */
template <typename T>
T pCM_sqr_from_s(const T s, const T mass_a, const T mass_b) noexcept {
  const auto mass_a_sqr = mass_a * mass_a;
  const auto x = s + mass_a_sqr - mass_b * mass_b;
  return x * x * (T(0.25) / s) - mass_a_sqr;
}

/**
 * \return The center-of-mass momentum of two particles,
 * given s and their masses. [GeV]
 * \param[in] s Mandelstam s of the process [GeV^2].
 * \param[in] mass_a Mass of first particle [GeV].
 * \param[in] mass_b Mass of second particle [GeV].
 */
template <typename T>
T pCM_from_s(const T s, const T mass_a, const T mass_b) noexcept {
  const auto psqr = pCM_sqr_from_s(s, mass_a, mass_b);
  return psqr > T(0.) ? std::sqrt(psqr) : T(0.);
}

/**
 * \return The center-of-mass momentum of two particles,
 * given sqrt(s) and their masses. [GeV]
 * \param[in] sqrts sqrt(s) of the process [GeV].
 * \param[in] mass_a Mass of first particle [GeV].
 * \param[in] mass_b Mass of second particle [GeV].
 */
template <typename T>
T pCM(const T sqrts, const T mass_a, const T mass_b) noexcept {
  return pCM_from_s(sqrts * sqrts, mass_a, mass_b);
}

/**
 * \return The squared center-of-mass momentum of two particles,
 * given sqrt(s) and their masses.
 * \param[in] sqrts sqrt(s) of the process [GeV].
 * \param[in] mass_a Mass of first particle [GeV].
 * \param[in] mass_b Mass of second particle [GeV].
 */
template <typename T>
T pCM_sqr(const T sqrts, const T mass_a, const T mass_b) noexcept {
  return pCM_sqr_from_s(sqrts * sqrts, mass_a, mass_b);
}

/**
 * Get the range of Mandelstam-t values allowed in a particular 2->2 process,
 * see PDG 2014 booklet, eq. (46.34).
 * \param[in] sqrts sqrt(s) of the process [GeV].
 * \param[in] m1 Mass of first incoming particle [GeV].
 * \param[in] m2 Mass of second incoming particle [GeV].
 * \param[in] m3 Mass of first outgoing particle [GeV].
 * \param[in] m4 Mass of second outgoing particle [GeV].
 * \return array consisting of {t_min, t_max}
 *
 * Note that both t_min and t_max are negative,
 * with |t_min| < |t_max|, i.e. t_min > t_max.
 */
template <typename T>
std::array<T, 2> get_t_range(const T sqrts, const T m1, const T m2, const T m3,
                             const T m4) {
  const T p_i = pCM(sqrts, m1, m2);  // initial-state CM momentum
  const T p_f = pCM(sqrts, m3, m4);  // final-state CM momentum
  const T sqrt_t0 = (m1 * m1 - m2 * m2 - m3 * m3 + m4 * m4) / (2. * sqrts);
  const T t0 = sqrt_t0 * sqrt_t0;
  const T t_min = t0 - (p_i - p_f) * (p_i - p_f);
  const T t_max = t0 - (p_i + p_f) * (p_i + p_f);
  assert(t_min >= t_max);
  return {t_min, t_max};
}

/**
 * Helper function for plab_from_s.
 * \param[in] mandelstam_s The Mandelstam variable s [GeV^2]
 * \param[in] m_sum Sum of masses of target and projectile [GeV]
 * \f$ m_1 + m_2 \f$
 */
static inline void check_energy(double mandelstam_s, double m_sum) {
  if (mandelstam_s < m_sum * m_sum) {
    std::stringstream err;
    err << "plab_from_s: s too small: " << mandelstam_s << " < "
        << m_sum * m_sum;
    throw std::runtime_error(err.str());
  }
}

/**
 * Helper function for plab_from_s.
 * \param[in] mandelstam_s The Mandelstam variable s [GeV^2]
 * \param[in] radicand \f$ (s - (m_1 + m_2)^2) * (s - (m_1 - m_2)^2) \f$
 * where \f$ m_1 \f$ and \f$ m_2 \f$ are masses of incoming particles [GeV^4]
 */
static inline void check_radicand(double mandelstam_s, double radicand) {
  if (radicand < 0) {
    std::stringstream err;
    err << "plab_from_s: negative radicand: " << mandelstam_s;
    throw std::runtime_error(err.str());
  }
}

/**
 * Convert Mandelstam-s to p_lab in a fixed-target collision.
 * This assumes both particles have the given mass.
 * \param[in] mandelstam_s The Mandelstam variable s [GeV^2]
 * \param[in] mass Mass of projectile and target [GeV]
 * \return Momentum of the projectile in the lab frame [GeV]
 */
inline double plab_from_s(double mandelstam_s, double mass) {
  const double radicand = mandelstam_s * (mandelstam_s - 4 * mass * mass);
#ifndef NDEBUG
  const double m_sum = 2 * mass;
  check_energy(mandelstam_s, m_sum);
  check_radicand(mandelstam_s, radicand);
#endif
  return std::sqrt(radicand) / (2 * mass);
}

/**
 * Convert Mandelstam-s to p_lab in a fixed-target collision.
 * This assumes both particles have the mass of a nucleon.
 * \param[in] mandelstam_s The Mandelstam variable s [GeV^2]
 * \return Momentum of the projectile in the lab frame [GeV]
 */
inline double plab_from_s(double mandelstam_s) {
  return plab_from_s(mandelstam_s, nucleon_mass);
}

/**
 * Convert Mandelstam-s to p_lab in a fixed-target collision.
 * The mass of the projectile and the mass of the target have to be given.
 * \param[in] mandelstam_s the Mandelstam variable s [GeV^2]
 * \param[in] m_projectile mass of the projectile [GeV]
 * \param[in] m_target mass of the target [GeV]
 * \return momentum of the projectile in the lab frame [GeV]
 */
inline double plab_from_s(double mandelstam_s, double m_projectile,
                          double m_target) {
  const double m_sum = m_projectile + m_target;
  const double m_diff = m_projectile - m_target;
  const double radicand =
      (mandelstam_s - m_sum * m_sum) * (mandelstam_s - m_diff * m_diff);
/* This is equivalent to:
 * const double radicand
 *     = (mandelstam_s - m_a_sq - m_b_sq) * (mandelstam_s - m_a_sq - m_b_sq)
 *     - 4 * m_a_sq * m_b_sq; */
#ifndef NDEBUG
  check_energy(mandelstam_s, m_sum);
  check_radicand(mandelstam_s, radicand);
#endif
  return std::sqrt(radicand) / (2 * m_target);
}

/**
 * Convert E_tot to Mandelstam-s for a fixed-target setup,
 * with a projectile of mass m_P and a total energy e_tot
 * and a target of mass m_T at rest.
 * \param[in] e_tot energy of the projectile in the lab frame [GeV]
 * \param[in] m_P mass of the projectile [GeV]
 * \param[in] m_T mass of the target [GeV]
 * \return The Mandelstam variable s [GeV^2]
 */
inline double s_from_Etot(double e_tot, double m_P, double m_T) {
  return m_P * m_P + m_T * m_T + 2 * m_T * e_tot;
}
/**
 * Convert E_tot to Mandelstam-s for two beams with total energies and
 * masses (E,m)
 *
 * \param[in] e_tot_p Total energy of projectile [GeV]
 * \param[in] e_tot_t Total energy of target [GeV]
 * \param[in] m_p     Mass of projectile [GeV]
 * \param[in] m_t     Mass of target     [GeV]
 * \return Mandelstam-s [GeV^2]
 */
inline double s_from_Etot(double e_tot_p, double e_tot_t, double m_p,
                          double m_t) {
  double pz_p = std::sqrt(e_tot_p * e_tot_p - m_p * m_p);
  double pz_t = std::sqrt(e_tot_t * e_tot_t - m_t * m_t);
  return std::pow(e_tot_p + e_tot_t, 2) - std::pow(pz_p - pz_t, 2);
}
/**
 * Convert E_kin to Mandelstam-s for a fixed-target setup,
 * with a projectile of mass m_P and a kinetic energy e_kin
 * and a target of mass m_T at rest.
 * \param[in] e_kin kinetic energy of the projectile in the lab frame [GeV]
 * \param[in] m_P mass of the projectile [GeV]
 * \param[in] m_T mass of the target [GeV]
 * \return The Mandelstam variable s [GeV^2]
 */
inline double s_from_Ekin(double e_kin, double m_P, double m_T) {
  return s_from_Etot(e_kin + m_P, m_P, m_T);
}
/**
 * Convert E_kin=(E_tot-m) to Mandelstam-s for two beams with total
 * energies and masses (E,m)
 *
 * \param[in] e_kin_p Kinetic energy of projectile [GeV]
 * \param[in] e_kin_t Kinetic energy of target [GeV]
 * \param[in] m_p     Mass of projectile [GeV]
 * \param[in] m_t     Mass of target     [GeV]
 * \return Mandelstam-s [GeV^2]
 */
inline double s_from_Ekin(double e_kin_p, double e_kin_t, double m_p,
                          double m_t) {
  return s_from_Etot(e_kin_p + m_t, e_kin_t + m_t, m_p, m_t);
}
/**
 * Convert p_lab to Mandelstam-s for a fixed-target setup,
 * with a projectile of mass m_P and momentum plab
 * and a target of mass m_T at rest.
 * \param[in] plab Momentum of the projectile in the lab frame [GeV]
 * \param[in] m_P Mass of the projectile [GeV]
 * \param[in] m_T Mass of the target [GeV]
 * \return The Mandelstam variable s [GeV^2]
 */
inline double s_from_plab(double plab, double m_P, double m_T) {
  return m_P * m_P + m_T * m_T + 2 * m_T * std::sqrt(m_P * m_P + plab * plab);
}
/**
 * Convert P_lab to Mandelstam-s for two beams with total momenta and masses
 * (P,m) (P_lab gives per nucleon, P=P_lab*A)
 *
 * \param[in] plab_p Kinetic energy of projectile [GeV]
 * \param[in] plab_t Kinetic energy of target [GeV]
 * \param[in] m_p    Mass of projectile [GeV]
 * \param[in] m_t    Mass of target     [GeV]
 * \return Mandelstam-s [GeV^2]
 */
inline double s_from_plab(double plab_p, double plab_t, double m_p,
                          double m_t) {
  return s_from_Etot(std::sqrt(m_p * m_p + plab_p * plab_p),
                     std::sqrt(m_t * m_t + plab_t * plab_t), plab_p, plab_t);
}

}  // namespace smash

#endif  // SRC_INCLUDE_SMASH_KINEMATICS_H_
