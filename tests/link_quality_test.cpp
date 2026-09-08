// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

#include <gtest/gtest.h>

#include <cstdint>

#include "crsf/crsf.hpp"

TEST(Ewma, ConvergesOnAConstantSample)
{
  crsf::Ewma<4> filter;
  for (int i = 0; i < 200; ++i) {
    filter.update(100);
  }
  EXPECT_EQ(filter.value(), 100U);
}

TEST(Ewma, HoldsSteadyOnceConverged)
{
  crsf::Ewma<4> filter;
  filter.reset(50);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(filter.update(50), 50U);
  }
}

TEST(Ewma, ResetJumpsImmediately)
{
  crsf::Ewma<6> filter;
  filter.reset(75);
  EXPECT_EQ(filter.value(), 75U);
}

TEST(Ewma, SmallerShiftReactsFaster)
{
  crsf::Ewma<2> fast;
  crsf::Ewma<6> slow;
  for (int i = 0; i < 8; ++i) {
    fast.update(100);
    slow.update(100);
  }
  EXPECT_GT(fast.value(), slow.value());
}

TEST(Ewma, DecaysTowardZero)
{
  crsf::Ewma<3> filter;
  filter.reset(100);
  for (int i = 0; i < 200; ++i) {
    filter.update(0);
  }
  EXPECT_EQ(filter.value(), 0U);
}

TEST(Ewma, IsUsableInConstexprContext)
{
  static_assert([] {
    crsf::Ewma<3> filter;
    filter.reset(40);
    return filter.value();
  }() == 40U);
  SUCCEED();
}

TEST(LinkQuality, StartsAtZeroRatherThanAssumingAGoodLink)
{
  const crsf::LinkQuality<4> quality;
  EXPECT_EQ(quality.percent(), 0U);
}

TEST(LinkQuality, PerfectLinkClimbsToFull)
{
  crsf::LinkQuality<4> quality;
  for (int i = 0; i < 200; ++i) {
    quality.on_frame(true);
  }
  EXPECT_EQ(quality.percent(), crsf::kLinkQualityMax);
}

// The case that motivates on_missed(): a dead link delivers no frames to sample.
TEST(LinkQuality, DeadLinkDecaysToZero)
{
  crsf::LinkQuality<4> quality;
  for (int i = 0; i < 200; ++i) {
    quality.on_frame(true);
  }
  ASSERT_EQ(quality.percent(), crsf::kLinkQualityMax);

  for (int i = 0; i < 200; ++i) {
    quality.on_missed();
  }
  EXPECT_EQ(quality.percent(), 0U);
}

TEST(LinkQuality, CrcFailuresLowerQualityLikeMissedFrames)
{
  crsf::LinkQuality<4> corrupted;
  crsf::LinkQuality<4> missed;
  corrupted.reset(crsf::kLinkQualityMax);
  missed.reset(crsf::kLinkQualityMax);

  for (int i = 0; i < 20; ++i) {
    corrupted.on_frame(false);
    missed.on_missed();
  }
  EXPECT_EQ(corrupted.percent(), missed.percent());
  EXPECT_LT(corrupted.percent(), crsf::kLinkQualityMax);
}

TEST(LinkQuality, IntermittentLinkSettlesBetweenTheExtremes)
{
  crsf::LinkQuality<4> quality;
  for (int i = 0; i < 400; ++i) {
    quality.on_frame(i % 2 == 0);
  }
  EXPECT_GT(quality.percent(), 20U);
  EXPECT_LT(quality.percent(), 80U);
}
