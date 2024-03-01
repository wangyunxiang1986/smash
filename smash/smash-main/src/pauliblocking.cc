/*
 *
 *    Copyright (c) 2015-2020,2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "smash/pauliblocking.h"

#include "smash/constants.h"
#include "smash/logging.h"

namespace smash {
static constexpr int LPauliBlocking = LogArea::PauliBlocking::id;

PauliBlocker::PauliBlocker(Configuration conf,
                           const ExperimentParameters &param)
    : sig_(param.gaussian_sigma),
      rc_(conf.take({"Gaussian_Cutoff"}, 2.2)),
      rr_(conf.take({"Spatial_Averaging_Radius"}, 1.86)),
      rp_(conf.take({"Momentum_Averaging_Radius"}, 0.08)),
      ntest_(param.testparticles),
      n_ensembles_(param.n_ensembles) {
  if (ntest_ * n_ensembles_ < 20) {
    logg[LPauliBlocking].warn(
        "Phase-space density calculation in Pauli blocking will not work "
        "reasonably.\nEither use testparticles or ensembles, or both.\n"
        "The recommended testparticles * ensembles is at least 20.");
  }

  if (rc_ < rr_ || rr_ < 0.0 || rp_ < 0) {
    logg[LPauliBlocking].error(
        "Please choose reasonable parameters for Pauli blocking:\n"
        "All radii have to be positive and Gaussian_Cutoff should\n"
        "be larger than Spatial_Averaging_Radius");
  }

  init_weights_analytical();
}

PauliBlocker::~PauliBlocker() {}

double PauliBlocker::phasespace_dens(const ThreeVector &r, const ThreeVector &p,
                                     const std::vector<Particles> &ensembles,
                                     const PdgCode pdg,
                                     const ParticleList &disregard) const {
  double f = 0.0;

  /* TODO(oliiny): looping over all particles is inefficient,
   * I need only particles within rp_ radius in momentum and
   * within rr_+rc_ in coordinate space. Some search algorithm might help. */
  for (const Particles &particles : ensembles) {
    for (const ParticleData &part : particles) {
      // Only consider identical particles
      if (part.pdgcode() != pdg) {
        continue;
      }
      // Only consider momenta in sphere of radius rp_ with center at p
      const double pdist_sqr = (part.momentum().threevec() - p).sqr();
      if (pdist_sqr > rp_ * rp_) {
        continue;
      }
      const double rdist_sqr = (part.position().threevec() - r).sqr();
      // Only consider coordinates in sphere of radius rr_+rc_ with center at r
      if (rdist_sqr >= (rr_ + rc_) * (rr_ + rc_)) {
        continue;
      }
      // Do not count particles that should be disregarded.
      bool to_disregard = false;
      for (const auto &disregard_part : disregard) {
        if (part.id() == disregard_part.id()) {
          to_disregard = true;
        }
      }
      if (to_disregard) {
        continue;
      }
      // 1st order interpolation using tabulated values
      const double i_real =
          std::sqrt(rdist_sqr) / (rr_ + rc_) * weights_.size();
      const size_t i = std::floor(i_real);
      const double rest = i_real - i;
      if (likely(i + 1 < weights_.size())) {
        f += weights_[i] * rest + weights_[i + 1] * (1. - rest);
      }
    }  // loop over particles in one ensemble
  }    // loop over ensembles
  return f / ntest_ / n_ensembles_;
}

void PauliBlocker::init_weights_analytical() {
  const double pi = M_PI;
  const double sqrt2 = std::sqrt(2.);
  const double sqrt_2pi = std::sqrt(2. * pi);
  // Volume of the phase-space area; Factor 2 stands for spin.
  const double phase_volume =
      2 * (4. / 3. * pi * rr_ * rr_ * rr_) * (4. / 3. * pi * rp_ * rp_ * rp_) /
      ((2 * pi * hbarc) * (2 * pi * hbarc) * (2 * pi * hbarc));
  // Analytical expression for integral in denominator
  const double norm =
      std::erf(rc_ / sqrt2 / sig_) -
      rc_ * 2 / sqrt_2pi / sig_ * std::exp(-0.5 * rc_ * rc_ / sig_ / sig_);

  double integral;
  // Step of the table for tabulated integral
  const double d_pos = (rr_ + rc_) / static_cast<double>(weights_.size());

  for (size_t k = 0; k < weights_.size(); k++) {
    // rdist = 0 ... rc_ (gauss cut) + rr_ (position cut)
    const double rj = d_pos * k;
    if (rj < really_small) {
      // Assuming rc_ > rr_
      const double A = rr_ / sqrt2 / sig_;
      integral = sqrt_2pi * sig_ * std::erf(A) - 2 * rr_ * std::exp(-A * A);
      integral *= sig_ * sig_;
    } else if (rc_ > rj + rr_) {
      const double A = (rj + rr_) / sqrt2 / sig_;
      const double B = (rj - rr_) / sqrt2 / sig_;
      integral = sig_ / rj * (std::exp(-A * A) - std::exp(-B * B)) +
                 0.5 * sqrt_2pi * (std::erf(A) - std::erf(B));
      integral *= sig_ * sig_ * sig_;
    } else {
      const double A = rc_ / sqrt2 / sig_;
      const double B = (rj - rr_) / sqrt2 / sig_;
      const double C = (rc_ - rj) * (rc_ - rj) - rr_ * rr_ + 2 * sig_ * sig_;
      integral =
          (0.5 * std::exp(-A * A) * C - sig_ * sig_ * std::exp(-B * B)) / rj +
          0.5 * sqrt_2pi * sig_ * (std::erf(A) - std::erf(B));
      integral *= sig_ * sig_;
    }
    integral *= 2 * pi / std::pow(2 * pi * sig_ * sig_, 1.5);
    weights_[k] = integral / norm / phase_volume;
    logg[LPauliBlocking].debug("Analytical weights[", k, "] = ", weights_[k]);
  }
}

}  // namespace smash
