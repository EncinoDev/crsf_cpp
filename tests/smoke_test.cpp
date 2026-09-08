// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

#include <gtest/gtest.h>

#include "crsf/version.hpp"

TEST(CrsfCppSmoke, VersionConstantsExposed)
{
  EXPECT_EQ(crsf::kVersionMajor, 0U);
  EXPECT_EQ(crsf::kVersionMinor, 1U);
}
