#pragma once

#include "engine/resource/animation_clip.h"
#include "engine/resource/skeleton.h"

namespace engine {

/**
 * @brief Maps @p clip, read for @p source, onto @p target by joint name, so
 * clip packs drive any mesh on a rig with the same names.
 *
 * Rotations carry over unchanged. Translations keep the target's own bone
 * lengths: roots keep their translation keys, a root's children (the hips)
 * have theirs scaled by the ratio of the two rest lengths, and every other
 * joint holds its target rest translation and scale. Target joints the clip
 * doesn't name hold their rest pose. When @p source and @p target are the
 * same rig (names and rest translations), the clip is returned unchanged.
 *
 * @return A clip whose tracks index @p target.
 * @throws std::runtime_error If no joint of @p clip matches a target name.
 */
auto RetargetClip(const AnimationClip& clip, const Skeleton& source,
                  const Skeleton& target) -> AnimationClip;

}  // namespace engine
