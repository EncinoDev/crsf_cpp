#include <gtest/gtest.h>

#include "crsf/version.hpp"

TEST(CrsfCppSmoke, VersionConstantsExposed)
{
  EXPECT_EQ(crsf::kVersionMajor, 0u);
  EXPECT_EQ(crsf::kVersionMinor, 1u);
}
