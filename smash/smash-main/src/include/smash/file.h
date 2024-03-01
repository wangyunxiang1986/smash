/*
 *
 *    Copyright (c) 2014,2017-2018,2020,2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#ifndef SRC_INCLUDE_SMASH_FILE_H_
#define SRC_INCLUDE_SMASH_FILE_H_

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include "forwarddeclarations.h"

namespace smash {

/**
 * FileDeleter is the deleter class for std::unique_ptr of std::FILE.
 *
 * std::unique_ptr takes a second template argument which determines what
 * happens when the resource it holds needs to be freed. The default
 * implementation calls `delete`. For std::FILE the resource needs to be freed
 * with a call to std::fclose instead. Therefore FilePtr requires a custom
 * deleter class to correctly free the resource.
 */
struct FileDeleter {
  /// The class has no members, so this is a noop.
  constexpr FileDeleter() = default;

  /** Frees the std::FILE resource if it is non-zero.
   *
   * \param[in] f File resource.
   * \throws runtime_error if the file could not get closed.
   */
  void operator()(std::FILE* f) const {
    if (f == nullptr) {
      return;
    }
    if (0 != std::fclose(f)) {
      throw std::runtime_error(std::strerror(errno));
    }
  }
};

/**
 * A RAII type to replace `std::FILE *`.
 *
 * This is an alias type for std::unique_ptr to automatically free the std::FILE
 * resource after the last reference goes out of scope. It is important to use a
 * custom deleter type, and therefore SMASH code should never use
 * std::unique_ptr directly with std::FILE.
 */
using FilePtr = std::unique_ptr<std::FILE, FileDeleter>;

/**
 * A RAII type to replace `std::FILE *`.
 *
 * While open, the file name ends with ".unfinished".
 *
 * Automatically closes and renames the file to the original when it goes out of
 * scope. If the object is destroyed because of stack unwinding, no renaming
 * is done.
 */
class RenamingFilePtr {
 public:
  /**
   * Construct a `RenamingFilePtr`.
   *
   * \param[in] filename Path to the file.
   * \param[in] mode The mode in which the file should be opened (see
   *                 `std::fopen`).
   * \return The constructed object.
   */
  RenamingFilePtr(const std::filesystem::path& filename,
                  const std::string& mode);
  /// Get the underlying `FILE*` pointer.
  FILE* get();
  /// Close the file and rename it.
  ~RenamingFilePtr();

 private:
  /// Internal file pointer.
  FILE* file_;
  /// Path of the finished file.
  std::filesystem::path filename_;
  /// Path of the unfinished file.
  std::filesystem::path filename_unfinished_;
  /**
   * Number of uncaught exceptions at the time when the object is created.
   * If it has not changed when the object is destroyed, we suppose that
   * the contents of the file are reliable and in the destructor we can safely
   * rename the file and remove the _.unfinished_ suffix.
   */
  int uncaught_exceptions_{std::uncaught_exceptions()};
};

/**
 * Open a file with given mode.
 *
 * This wraps std::fopen but uses FileDeleter to automatically close the file.
 *
 * \param[in] filename Path to the file.
 * \param[in] mode The mode in which the file should be opened (see
 *                 `std::fopen`).
 * \return The constructed `FilePtr`.
 */
FilePtr fopen(const std::filesystem::path& filename, const std::string& mode);

}  // namespace smash

#endif  // SRC_INCLUDE_SMASH_FILE_H_
