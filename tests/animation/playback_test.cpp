#include "engine/animation/playback.h"

#include <gtest/gtest.h>

#include "engine/resource/animation_handles.h"

namespace engine::animation {

namespace {

/// Two stand-in clips; playback only compares handles.
constexpr ClipHandle kWalk(0);
constexpr ClipHandle kRun(1);

TEST(Playback, LoopingTrackWraps) {
  Track track{.clip = kWalk, .phase = 0.9F};
  Advance(track, 0.5F, 2.0F);
  EXPECT_NEAR(track.phase, 0.15F, 1e-5F);
  EXPECT_FALSE(IsFinished(track));
}

TEST(Playback, OneShotHoldsItsEndAndFinishes) {
  Track track{.clip = kWalk, .phase = 0.9F, .loop = false};
  Advance(track, 0.5F, 2.0F);
  EXPECT_FLOAT_EQ(track.phase, 1.0F);
  EXPECT_TRUE(IsFinished(track));
}

TEST(Playback, SpeedScalesTheRate) {
  Track track{.clip = kWalk, .speed = 2.0F};
  Advance(track, 0.25F, 2.0F);
  EXPECT_NEAR(track.phase, 0.25F, 1e-5F);
}

TEST(Playback, EmptyTrackStaysPut) {
  Track track{};
  Advance(track, 1.0F, 1.0F);
  EXPECT_FLOAT_EQ(track.phase, 0.0F);
  EXPECT_FALSE(IsFinished(track));
}

TEST(Playback, FadeHandsOverAndEnds) {
  LayerState layer{};
  StartTrack(layer, {.clip = kWalk}, 0.3F);
  // Nothing was playing, so nothing fades
  EXPECT_FALSE(layer.previous.clip.IsValid());
  EXPECT_FLOAT_EQ(FadeAlpha(layer), 1.0F);

  StartTrack(layer, {.clip = kRun}, 0.4F);
  EXPECT_EQ(layer.previous.clip, kWalk);
  EXPECT_EQ(layer.current.clip, kRun);
  AdvanceFade(layer, 0.1F);
  EXPECT_NEAR(FadeAlpha(layer), 0.25F, 1e-5F);
  AdvanceFade(layer, 0.3F);
  EXPECT_FALSE(layer.previous.clip.IsValid());
  EXPECT_FLOAT_EQ(FadeAlpha(layer), 1.0F);
}

TEST(Playback, ZeroFadeCuts) {
  LayerState layer{};
  StartTrack(layer, {.clip = kWalk}, 0.0F);
  StartTrack(layer, {.clip = kRun}, 0.0F);
  EXPECT_FALSE(layer.previous.clip.IsValid());
  EXPECT_EQ(layer.current.clip, kRun);
}

}  // namespace

}  // namespace engine::animation
