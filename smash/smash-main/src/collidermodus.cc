/*
 *    Copyright (c) 2012-2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 */

#include "smash/collidermodus.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "smash/configuration.h"
#include "smash/customnucleus.h"
#include "smash/experimentparameters.h"
#include "smash/fourvector.h"
#include "smash/logging.h"
#include "smash/random.h"

namespace smash {
static constexpr int LCollider = LogArea::Collider::id;

ColliderModus::ColliderModus(Configuration modus_config,
                             const ExperimentParameters &params) {
  Configuration modus_cfg =
      modus_config.extract_sub_configuration({"Collider"});
  // Get the reference frame for the collision calculation.
  if (modus_cfg.has_value({"Calculation_Frame"})) {
    frame_ = modus_cfg.take({"Calculation_Frame"});
  }

  Configuration proj_cfg = modus_cfg.extract_sub_configuration({"Projectile"});
  Configuration targ_cfg = modus_cfg.extract_sub_configuration({"Target"});
  /* Needed to check if projectile and target in customnucleus are read from
   * the same input file.*/
  bool same_file = false;
  // Set up the projectile nucleus
  if (proj_cfg.has_value({"Deformed"})) {
    projectile_ =
        create_deformed_nucleus(proj_cfg, params.testparticles, "projectile");
  } else if (proj_cfg.has_value({"Custom"})) {
    same_file = same_inputfile(proj_cfg, targ_cfg);
    projectile_ = std::make_unique<CustomNucleus>(
        proj_cfg, params.testparticles, same_file);
  } else {
    projectile_ = std::make_unique<Nucleus>(proj_cfg, params.testparticles);
  }
  if (projectile_->size() < 1) {
    throw ColliderEmpty("Input Error: Projectile nucleus is empty.");
  }
  projectile_->set_label(BelongsTo::Projectile);

  // Set up the target nucleus
  if (targ_cfg.has_value({"Deformed"})) {
    target_ = create_deformed_nucleus(targ_cfg, params.testparticles, "target");
  } else if (targ_cfg.has_value({"Custom"})) {
    target_ = std::make_unique<CustomNucleus>(targ_cfg, params.testparticles,
                                              same_file);
  } else {
    target_ = std::make_unique<Nucleus>(targ_cfg, params.testparticles);
  }
  if (target_->size() < 1) {
    throw ColliderEmpty("Input Error: Target nucleus is empty.");
  }
  target_->set_label(BelongsTo::Target);

  // Get the Fermi-Motion input (off, on, frozen)
  if (modus_cfg.has_value({"Fermi_Motion"})) {
    fermi_motion_ = modus_cfg.take({"Fermi_Motion"}, FermiMotion::Off);
  }

  // Get the total nucleus-nucleus collision energy. Since there is
  // no meaningful choice for a default energy, we require the user to
  // give one (and only one) energy input from the available options.
  int energy_input = 0;
  const double mass_projec = projectile_->mass();
  const double mass_target = target_->mass();
  // average mass of a particle in that nucleus
  const double mass_a =
      projectile_->mass() / projectile_->number_of_particles();
  const double mass_b = target_->mass() / target_->number_of_particles();
  // Option 1: Center of mass energy.
  if (modus_cfg.has_value({"Sqrtsnn"})) {
    sqrt_s_NN_ = modus_cfg.take({"Sqrtsnn"});
    // Check that input satisfies the lower bound (everything at rest).
    if (sqrt_s_NN_ <= mass_a + mass_b) {
      throw ModusDefault::InvalidEnergy(
          "Input Error: sqrt(s_NN) is not larger than masses:\n" +
          std::to_string(sqrt_s_NN_) + " GeV <= " + std::to_string(mass_a) +
          " GeV + " + std::to_string(mass_b) + " GeV.");
    }
    // Set the total nucleus-nucleus collision energy.
    total_s_ = (sqrt_s_NN_ * sqrt_s_NN_ - mass_a * mass_a - mass_b * mass_b) *
                   mass_projec * mass_target / (mass_a * mass_b) +
               mass_projec * mass_projec + mass_target * mass_target;
    energy_input++;
  }
  /* Option 2: Total energy per nucleon of the projectile nucleus
   * (target at rest).  */
  if (modus_cfg.has_value({"E_Tot"})) {
    const double e_tot = modus_cfg.take({"E_Tot"});
    if (e_tot < 0) {
      throw ModusDefault::InvalidEnergy(
          "Input Error: "
          "E_Tot must be nonnegative.");
    }
    // Set the total nucleus-nucleus collision energy.
    total_s_ = s_from_Etot(e_tot * projectile_->number_of_particles(),
                           mass_projec, mass_target);
    sqrt_s_NN_ = std::sqrt(s_from_Etot(e_tot, mass_a, mass_b));
    energy_input++;
  }
  /* Option 3: Kinetic energy per nucleon of the projectile nucleus
   * (target at rest).  */
  if (modus_cfg.has_value({"E_Kin"})) {
    const double e_kin = modus_cfg.take({"E_Kin"});
    if (e_kin < 0) {
      throw ModusDefault::InvalidEnergy(
          "Input Error: "
          "E_Kin must be nonnegative.");
    }
    // Set the total nucleus-nucleus collision energy.
    total_s_ = s_from_Ekin(e_kin * projectile_->number_of_particles(),
                           mass_projec, mass_target);
    sqrt_s_NN_ = std::sqrt(s_from_Ekin(e_kin, mass_a, mass_b));
    energy_input++;
  }
  // Option 4: Momentum of the projectile nucleus (target at rest).
  if (modus_cfg.has_value({"P_Lab"})) {
    const double p_lab = modus_cfg.take({"P_Lab"});
    if (p_lab < 0) {
      throw ModusDefault::InvalidEnergy(
          "Input Error: "
          "P_Lab must be nonnegative.");
    }
    // Set the total nucleus-nucleus collision energy.
    total_s_ = s_from_plab(p_lab * projectile_->number_of_particles(),
                           mass_projec, mass_target);
    sqrt_s_NN_ = std::sqrt(s_from_plab(p_lab, mass_a, mass_b));
    energy_input++;
  }
  // Option 5: Total energy per nucleon of _each_ beam
  if (proj_cfg.has_value({"E_Tot"}) && targ_cfg.has_value({"E_Tot"})) {
    const double e_tot_p = proj_cfg.take({"E_Tot"});
    const double e_tot_t = targ_cfg.take({"E_Tot"});
    if (e_tot_p < 0 || e_tot_t < 0) {
      throw ModusDefault::InvalidEnergy(
          "Input Error: "
          "E_Tot must be nonnegative.");
    }
    total_s_ = s_from_Etot(e_tot_p * projectile_->number_of_particles(),
                           e_tot_t * target_->number_of_particles(),
                           mass_projec, mass_target);
    sqrt_s_NN_ = std::sqrt(s_from_Ekin(e_tot_p, e_tot_t, mass_a, mass_b));
    energy_input++;
  }
  // Option 6: Kinetic energy per nucleon of _each_ beam
  if (proj_cfg.has_value({"E_Kin"}) && targ_cfg.has_value({"E_Kin"})) {
    const double e_kin_p = proj_cfg.take({"E_Kin"});
    const double e_kin_t = targ_cfg.take({"E_Kin"});
    if (e_kin_p < 0 || e_kin_t < 0) {
      throw ModusDefault::InvalidEnergy(
          "Input Error: "
          "E_Kin must be nonnegative.");
    }
    total_s_ = s_from_Ekin(e_kin_p * projectile_->number_of_particles(),
                           e_kin_t * target_->number_of_particles(),
                           mass_projec, mass_target);
    sqrt_s_NN_ = std::sqrt(s_from_Ekin(e_kin_p, e_kin_t, mass_a, mass_b));
    energy_input++;
  }
  // Option 7: Momentum per nucleon of _each_ beam
  if (proj_cfg.has_value({"P_Lab"}) && targ_cfg.has_value({"P_Lab"})) {
    const double p_lab_p = proj_cfg.take({"P_Lab"});
    const double p_lab_t = targ_cfg.take({"P_Lab"});
    if (p_lab_p < 0 || p_lab_t < 0) {
      throw ModusDefault::InvalidEnergy(
          "Input Error: "
          "P_Lab must be nonnegative.");
    }
    total_s_ = s_from_plab(p_lab_p * projectile_->number_of_particles(),
                           p_lab_t * target_->number_of_particles(),
                           mass_projec, mass_target);
    sqrt_s_NN_ = std::sqrt(s_from_plab(p_lab_p, p_lab_t, mass_a, mass_b));
    energy_input++;
  }
  if (energy_input == 0) {
    throw std::domain_error(
        "Input Error: Non-existent collision energy. "
        "Please provide one of Sqrtsnn/E_Kin/P_Lab.");
  }
  if (energy_input > 1) {
    throw std::domain_error(
        "Input Error: Redundant collision energy. "
        "Please provide only one of Sqrtsnn/E_Kin/P_Lab.");
  }

  /* Impact parameter setting: Either "Value", "Range", "Max" or "Sample".
   * Unspecified means 0 impact parameter.*/
  if (modus_cfg.has_value({"Impact", "Value"})) {
    impact_ = modus_cfg.take({"Impact", "Value"});
    imp_min_ = impact_;
    imp_max_ = impact_;
  } else {
    // If impact is not supplied by value, inspect sampling parameters:
    if (modus_cfg.has_value({"Impact", "Sample"})) {
      sampling_ = modus_cfg.take({"Impact", "Sample"});
      if (sampling_ == Sampling::Custom) {
        if (!(modus_cfg.has_value({"Impact", "Values"}) ||
              modus_cfg.has_value({"Impact", "Yields"}))) {
          throw std::domain_error(
              "Input Error: Need impact parameter spectrum for custom "
              "sampling. "
              "Please provide Values and Yields.");
        }
        const std::vector<double> impacts =
            modus_cfg.take({"Impact", "Values"});
        const std::vector<double> yields = modus_cfg.take({"Impact", "Yields"});
        if (impacts.size() != yields.size()) {
          throw std::domain_error(
              "Input Error: Need as many impact parameter values as yields. "
              "Please make sure that Values and Yields have the same length.");
        }
        impact_interpolation_ = std::make_unique<InterpolateDataLinear<double>>(
            InterpolateDataLinear<double>(impacts, yields));

        const auto imp_minmax =
            std::minmax_element(impacts.begin(), impacts.end());
        imp_min_ = *imp_minmax.first;
        imp_max_ = *imp_minmax.second;
        yield_max_ = *std::max_element(yields.begin(), yields.end());
      }
    }
    if (modus_cfg.has_value({"Impact", "Range"})) {
      const std::array<double, 2> range = modus_cfg.take({"Impact", "Range"});
      imp_min_ = range[0];
      imp_max_ = range[1];
    }
    if (modus_cfg.has_value({"Impact", "Max"})) {
      imp_min_ = 0.0;
      imp_max_ = modus_cfg.take({"Impact", "Max"});
    }
  }
  /// \todo include a check that only one method of specifying impact is used
  // whether the direction of separation should be ramdomly smapled
  random_reaction_plane_ =
      modus_cfg.take({"Impact", "Random_Reaction_Plane"}, false);
  // Look for user-defined initial separation between nuclei.
  if (modus_cfg.has_value({"Initial_Distance"})) {
    initial_z_displacement_ = modus_cfg.take({"Initial_Distance"});
    // the displacement is half the distance (both nuclei are shifted
    // initial_z_displacement_ away from origin)
    initial_z_displacement_ /= 2.0;
  }

  if (fermi_motion_ == FermiMotion::On) {
    logg[LCollider].info() << "Fermi motion is ON.";
  } else if (fermi_motion_ == FermiMotion::Frozen) {
    logg[LCollider].info() << "FROZEN Fermi motion is on.";
  } else if (fermi_motion_ == FermiMotion::Off) {
    logg[LCollider].info() << "Fermi motion is OFF.";
  }
}

std::ostream &operator<<(std::ostream &out, const ColliderModus &m) {
  return out << "-- Collider Modus:\n"
             << "sqrt(S) (nucleus-nucleus) = "
             << format(std::sqrt(m.total_s_), "GeV\n")
             << "sqrt(S) (nucleon-nucleon) = " << format(m.sqrt_s_NN_, "GeV\n")
             << "Projectile:\n"
             << *m.projectile_ << "\nTarget:\n"
             << *m.target_;
}

std::unique_ptr<DeformedNucleus> ColliderModus::create_deformed_nucleus(
    Configuration &nucleus_cfg, int ntest, const std::string &nucleus_type) {
  bool automatic_deformation = nucleus_cfg.take({"Deformed", "Automatic"});
  bool was_any_beta_given = nucleus_cfg.has_value({"Deformed", "Beta_2"}) ||
                            nucleus_cfg.has_value({"Deformed", "Beta_3"}) ||
                            nucleus_cfg.has_value({"Deformed", "Beta_4"});
  bool was_any_deformation_parameter_given =
      was_any_beta_given || nucleus_cfg.has_value({"Deformed", "Gamma"});
  bool was_gamma_given_without_beta_2 =
      nucleus_cfg.has_value({"Deformed", "Gamma"}) &&
      !nucleus_cfg.has_value({"Deformed", "Beta_2"});

  if (automatic_deformation && was_any_deformation_parameter_given) {
    throw std::domain_error(
        "Automatic deformation of " + nucleus_type +
        " nucleus requested, but deformation parameter(s) were provided as"
        " well. Please, check the 'Deformed' section in your input file.");
  } else if (!automatic_deformation && !was_any_beta_given) {
    throw std::domain_error(
        "Manual deformation of " + nucleus_type +
        " nucleus requested, but no deformation beta parameter was provided."
        " Please, check the 'Deformed' section in your input file.");
  } else if (!automatic_deformation && was_gamma_given_without_beta_2) {
    throw std::domain_error(
        "Manual deformation of " + nucleus_type +
        " nucleus requested, but 'Gamma' parameter was provided without "
        "providing a value of 'Beta_2' having hence no deformation effect. "
        "Please, check the 'Deformed' section in your input file.");
  } else {
    return std::make_unique<DeformedNucleus>(nucleus_cfg, ntest,
                                             automatic_deformation);
  }
}

double ColliderModus::initial_conditions(Particles *particles,
                                         const ExperimentParameters &) {
  // Populate the nuclei with appropriately distributed nucleons.
  // If deformed, this includes rotating the nucleus.
  projectile_->arrange_nucleons();
  target_->arrange_nucleons();

  // Use the total mandelstam variable to get the frame-dependent velocity for
  // each nucleus. Position a is projectile, position b is target.
  double v_a, v_b;
  std::tie(v_a, v_b) =
      get_velocities(total_s_, projectile_->mass(), target_->mass());

  // If velocities are larger or equal to 1, throw an exception.
  if (v_a >= 1.0 || v_b >= 1.0) {
    throw std::domain_error(
        "Found velocity equal to or larger than 1 in "
        "ColliderModus::initial_conditions.\nConsider using "
        "the center of velocity reference frame.");
  }

  // Calculate the beam velocity of the projectile and the target, which will be
  // used to calculate the beam momenta in experiment.cc
  if (fermi_motion_ == FermiMotion::Frozen) {
    velocity_projectile_ = v_a;
    velocity_target_ = v_b;
  }

  // Generate Fermi momenta if necessary
  if (fermi_motion_ == FermiMotion::On ||
      fermi_motion_ == FermiMotion::Frozen) {
    // Frozen: Fermi momenta will be ignored during the propagation to
    // avoid that the nuclei will fly apart.
    projectile_->generate_fermi_momenta();
    target_->generate_fermi_momenta();
  } else if (fermi_motion_ == FermiMotion::Off) {
  } else {
    throw std::domain_error("Invalid Fermi_Motion input.");
  }

  // Boost the nuclei to the appropriate velocity.
  projectile_->boost(v_a);
  target_->boost(v_b);

  // Shift the nuclei into starting positions. Contracted spheres with
  // nuclear radii should touch exactly at t=0. Modus starts at negative
  // time corresponding to additional initial displacement.
  const double d_a = std::max(0., projectile_->get_diffusiveness());
  const double d_b = std::max(0., target_->get_diffusiveness());
  const double r_a = projectile_->get_nuclear_radius();
  const double r_b = target_->get_nuclear_radius();
  const double dz = initial_z_displacement_;

  const double simulation_time = -dz / std::abs(v_a);
  const double proj_z = -dz - std::sqrt(1.0 - v_a * v_a) * (r_a + d_a);
  const double targ_z =
      +dz * std::abs(v_b / v_a) + std::sqrt(1.0 - v_b * v_b) * (r_b + d_b);
  // rotation angle in the transverse plane
  const double phi =
      random_reaction_plane_ ? random::uniform(0.0, 2.0 * M_PI) : 0.0;

  projectile_->shift(proj_z, +impact_ / 2.0, simulation_time);
  target_->shift(targ_z, -impact_ / 2.0, simulation_time);

  // Put the particles in the nuclei into code particles.
  projectile_->copy_particles(particles);
  target_->copy_particles(particles);
  rotate_reaction_plane(phi, particles);
  return simulation_time;
}

void ColliderModus::rotate_reaction_plane(double phi, Particles *particles) {
  for (ParticleData &p : *particles) {
    ThreeVector pos = p.position().threevec();
    ThreeVector mom = p.momentum().threevec();
    pos.rotate_around_z(phi);
    mom.rotate_around_z(phi);
    p.set_3position(pos);
    p.set_3momentum(mom);
  }
}

void ColliderModus::sample_impact() {
  switch (sampling_) {
    case Sampling::Quadratic: {
      // quadratic sampling: Note that for bmin > bmax, this still yields
      // the correct distribution (however canonical() = 0 is then the
      // upper end, not the lower).
      impact_ = std::sqrt(imp_min_ * imp_min_ +
                          random::canonical() *
                              (imp_max_ * imp_max_ - imp_min_ * imp_min_));
    } break;
    case Sampling::Custom: {
      // rejection sampling based on given distribution
      assert(impact_interpolation_ != nullptr);
      double probability_random = 1;
      double probability = 0;
      double b;
      while (probability_random > probability) {
        b = random::uniform(imp_min_, imp_max_);
        probability = (*impact_interpolation_)(b) / yield_max_;
        assert(probability < 1.);
        probability_random = random::uniform(0., 1.);
      }
      impact_ = b;
    } break;
    case Sampling::Uniform: {
      // linear sampling. Still, min > max works fine.
      impact_ = random::uniform(imp_min_, imp_max_);
    }
  }
}

std::pair<double, double> ColliderModus::get_velocities(double s, double m_a,
                                                        double m_b) {
  double v_a = 0.0;
  double v_b = 0.0;
  // Frame dependent calculations of velocities. Assume v_a >= 0, v_b <= 0.
  switch (frame_) {
    case CalculationFrame::CenterOfVelocity:
      v_a = center_of_velocity_v(s, m_a, m_b);
      v_b = -v_a;
      break;
    case CalculationFrame::CenterOfMass: {
      // Compute center of mass momentum.
      double pCM = pCM_from_s(s, m_a, m_b);
      v_a = pCM / std::sqrt(m_a * m_a + pCM * pCM);
      v_b = -pCM / std::sqrt(m_b * m_b + pCM * pCM);
    } break;
    case CalculationFrame::FixedTarget:
      v_a = fixed_target_projectile_v(s, m_a, m_b);
      break;
    default:
      throw std::domain_error(
          "Invalid reference frame in "
          "ColliderModus::get_velocities.");
  }
  return std::make_pair(v_a, v_b);
}

std::string ColliderModus::custom_file_path(const std::string &file_directory,
                                            const std::string &file_name) {
  // make sure that path is correct even if the / at the end is missing
  if (file_directory.back() == '/') {
    return file_directory + file_name;
  } else {
    return file_directory + '/' + file_name;
  }
}

bool ColliderModus::same_inputfile(Configuration &proj_config,
                                   Configuration &targ_config) {
  /* Check if both nuclei are custom
   * Only check target as function is called after if statement for projectile.
   */
  if (!targ_config.has_value({"Custom"})) {
    return false;
  }
  std::string projectile_file_directory =
      proj_config.read({"Custom", "File_Directory"});
  std::string target_file_directory =
      targ_config.read({"Custom", "File_Directory"});
  std::string projectile_file_name = proj_config.read({"Custom", "File_Name"});
  std::string target_file_name = targ_config.read({"Custom", "File_Name"});
  // Check if files are the same for projectile and target
  std::string proj_path =
      custom_file_path(projectile_file_directory, projectile_file_name);
  std::string targ_path =
      custom_file_path(target_file_directory, target_file_name);
  if (proj_path == targ_path) {
    return true;
  } else {
    return false;
  }
}

}  // namespace smash
