/*
 *
 *    Copyright (c) 2015,2017-2018,2020,2022
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "vir/test.h"  // This include has to be first

#include "smash/filelock.h"

#include "setup.h"

using namespace smash;

static const std::filesystem::path testoutputpath =
    std::filesystem::absolute(SMASH_TEST_OUTPUT_PATH);

TEST(directory_is_created) {
  std::filesystem::create_directories(testoutputpath);
  VERIFY(std::filesystem::exists(testoutputpath));
}

TEST(lock_is_created) {
  const std::filesystem::path lockpath = testoutputpath / "lock";
  {
    FileLock lock(lockpath);
    VERIFY(lock.acquire());
    VERIFY(std::filesystem::exists(lockpath));
  }
  VERIFY(!std::filesystem::exists(lockpath));
}

TEST(lock_is_denied) {
  const std::filesystem::path lockpath = testoutputpath / "lock";
  {
    FileLock lock1(lockpath);
    FileLock lock2(lockpath);
    VERIFY(lock1.acquire());
    VERIFY(!lock2.acquire());
    VERIFY(std::filesystem::exists(lockpath));
  }
  VERIFY(!std::filesystem::exists(lockpath));
}
