/*
 *
 *    Copyright (c) 2014-2015,2017-2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#ifndef SRC_INCLUDE_SMASH_GRID_H_
#define SRC_INCLUDE_SMASH_GRID_H_

#include <array>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include "forwarddeclarations.h"
#include "particles.h"

namespace smash {

/// Identifies the mode of the Grid.
enum class GridOptions : char {
  /// Without ghost cells
  Normal = 0,
  /// With ghost cells for periodic boundaries
  PeriodicBoundaries = 1
};

/// Indentifies the strategy of determining the cell size.
enum class CellSizeStrategy : char {
  /// Look for optimal cell size.
  Optimal,

  /**
   * Make cells as large as possible.
   *
   * This means a single cell for normal boundary conditions and 8 cells
   * for periodic boundary conditions.
   */
  Largest
};

/**
 * Identifies whether the number of cells should be limited. For the geometric
 * criterion it makes sense to not have less than 1 particle in each cell, since
 * the grid cell search is an optimization and the cells can be always made
 * larger. For the stochastic collision criterion, the cell size is an
 * important calculation parameter, which should be kept constant.
 * The number of cells therefore cannot be limited as the medium grows even
 * though this might be inefficient for large systems.
 */
enum class CellNumberLimitation : char {
  /// No cell number limitation.
  None,

  /// Limit the number of cells to the number of particles
  ParticleNumber
};

/**
 * Base class for Grid to host common functions that do not depend on the
 * GridOptions parameter.
 */
class GridBase {
 public:
  /// A type to store the sizes
  typedef int SizeType;

 protected:
  /**
   * \return the minimum x,y,z coordinates and the largest dx,dy,dz distances of
   * the particles in \p particles.
   *
   * \param[in] particles Particles in the system
   */
  static std::pair<std::array<double, 3>, std::array<double, 3>>
  find_min_and_length(const Particles &particles);
};

/**
 * Abstracts a list of cells that partition the particles in the experiment into
 * regions of space that can interact / cannot interact.
 *
 * This class is used to construct a helper data structure to reduce the
 * combinatorics of finding particle pairs that could interact (scatter). It
 * takes a list of ParticleData objects and sorts them in such a way that it is
 * easy to look only at lists of particles that have a chance of interacting.
 *
 * \tparam Options This policy parameter determines whether ghost cells are
 * created to support periodic boundaries, or not.
 */
template <GridOptions Options = GridOptions::Normal>
class Grid : public GridBase {
 public:
  /**
   * Constructs a grid from the given particle list \p particles. It
   * automatically determines the necessary size for the grid from the positions
   * of the particles.
   *
   * \param[in] particles The particles to place onto the grid.
   * \param[in] min_cell_length The minimal length a cell must have.
   * \param[in] timestep_duration Duration of the timestep. It is necessary for
   * formation times treatment: if particle is fully or partially formed before
   * the end of the timestep, it has to be on the grid.
   * \param[in] limit Limitation of cell number
   * \param[in] include_unformed_particles include unformed particles from
                                              the grid (worsens runtime)
   * \param[in] strategy The strategy for determining the cell size
   */
  Grid(const Particles &particles, double min_cell_length,
       double timestep_duration, CellNumberLimitation limit,
       const bool include_unformed_particles = false,
       CellSizeStrategy strategy = CellSizeStrategy::Optimal)
      : Grid{find_min_and_length(particles),
             std::move(particles),
             min_cell_length,
             timestep_duration,
             limit,
             include_unformed_particles,
             strategy} {}

  /**
   * Constructs a grid with the given minimum grid coordinates and grid length.
   * If you need periodic boundaries you have to use this constructor to set the
   * correct length to use for wrapping particles around the borders.
   *
   * \param[in] min_and_length A pair consisting of the three min coordinates
   * and the three lengths.
   * \param[in] particles The particles to place onto the grid.
   * \param[in] min_cell_length The minimal length a cell must have.
   * \param[in] timestep_duration Duration of the timestep in fm.
   * \param[in] limit Limitation of cell number.
   * \param[in] include_unformed_particles include unformed particles from
                                           the grid (worsens runtime).
   * \param[in] strategy The strategy for determining the cell size.
   * \throws runtime_error if your box length is smaller than the grid length.
   */
  Grid(const std::pair<std::array<double, 3>, std::array<double, 3>>
           &min_and_length,
       const Particles &particles, double min_cell_length,
       double timestep_duration, CellNumberLimitation limit,
       const bool include_unformed_particles = false,
       CellSizeStrategy strategy = CellSizeStrategy::Optimal);

  /**
   * Iterates over all cells in the grid and calls the callback arguments with
   * a search cell and 0 to 13 neighbor cells.
   *
   * The neighbor cells are constructed like this:
   * - one cell at x+1
   * - three cells (x-1,x,x+1) at y+1
   * - nine cells (x-1, y-1)...(x+1, y+1) at z+1
   *
   * \param[in] search_cell_callback A callable called for/with every non-empty
   *                                 cell in the grid.
   * \param[in] neighbor_cell_callback A callable called for/with every
   *                              non-empty cell and adjacent cell combination.
   *                              For a periodic grid, the first argument will
   *                              be adjusted to wrap around the grid.
   */
  void iterate_cells(
      const std::function<void(const ParticleList &)> &search_cell_callback,
      const std::function<void(const ParticleList &, const ParticleList &)>
          &neighbor_cell_callback) const;

  /**
   * \return the volume of a single grid cell
   */
  double cell_volume() const { return cell_volume_; }

 private:
  /**
   * \return the one-dimensional cell-index from the 3-dim index \p x, \p y, \p
   * z.
   */
  SizeType make_index(SizeType x, SizeType y, SizeType z) const;

  /**
   * \return the one-dimensional cell-index from the 3-dim index \p idx.
   * This is a convenience overload for the above function.
   */
  SizeType make_index(std::array<SizeType, 3> idx) const {
    return make_index(idx[0], idx[1], idx[2]);
  }

  /// The 3 lengths of the complete grid. Used for periodic boundary wrapping.
  const std::array<double, 3> length_;

  /// The volume of a single cell.
  double cell_volume_;

  /// The number of cells in x, y, and z direction.
  std::array<int, 3> number_of_cells_;

  /// The cell storage.
  std::vector<ParticleList> cells_;
};

}  // namespace smash

#endif  // SRC_INCLUDE_SMASH_GRID_H_
