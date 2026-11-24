#include <gtest/gtest.h>
#include <math.h>
#include <string.h>
#include <vector>

#include "Scene/AnimationAssets.h"
#include "Scene/AnimationSystem.h"
#include "Scene/AssetManager.h"
#include "Scene/SceneComponents.h"
#include "Scene/SceneReflection.h"
#include "Scene/SkinningCore.h"
#include "Runtime/Scene/Private/Animation/AnimationFiles.h"
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"

namespace
{
template<typename T>
std::vector<char> archive(const T& object)
{
    ozz::io::MemoryStream stream;
    {
        ozz::io::OArchive output(&stream);
        output << object;
    }
    std::vector<char> bytes(stream.Size());
    stream.Seek(0, ozz::io::Stream::kSet);
    stream.Read(bytes.data(), bytes.size());
    return bytes;
}

void append(std::vector<char>& bytes, const void* pData, size_t size)
{
    bytes.insert(bytes.end(), (const char*)pData, (const char*)pData + size);
}

// A two-joint arm: the root at the origin and the tip one unit up +Y. The skin's vertices sit on the tip, and the clip
// turns the tip by 90 degrees about +Z over one second.
struct ArmFiles
{
    std::vector<char> skeleton;
    std::vector<char> clip;
    std::vector<char> mismatchedClip;
};

ArmFiles buildArm()
{
    ozz::animation::offline::RawSkeleton raw;
    raw.roots.resize(1);
    raw.roots[0].name = "root";
    raw.roots[0].transform = ozz::math::Transform::identity();
    raw.roots[0].children.resize(1);
    raw.roots[0].children[0].name = "tip";
    raw.roots[0].children[0].transform = ozz::math::Transform::identity();
    raw.roots[0].children[0].transform.translation = ozz::math::Float3(0, 1, 0);
    const ozz::unique_ptr<ozz::animation::Skeleton> skeleton = ozz::animation::offline::SkeletonBuilder()(raw);
    EXPECT_TRUE(skeleton && skeleton->num_joints() == 2 && strcmp(skeleton->joint_names()[1], "tip") == 0);

    ArmFiles                    files;
    const std::vector<char>     skeletonArchive = archive(*skeleton);
    const hz::SkeletonFileHeader skeletonHeader = { .skinCount = 1, .archiveSize = (uint32_t)skeletonArchive.size() };
    append(files.skeleton, &skeletonHeader, sizeof(skeletonHeader));
    const uint32_t jointCount = 2, joints[] = { 0, 1 };
    const float    inverseBind[2][16] = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },
                                          { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1 } };
    const float    bounds[2][6] = { { 0, 0, 0, 1, 0, 0 }, { 0, 0, 0, 1, 1, 0 } };
    append(files.skeleton, &jointCount, sizeof(jointCount));
    append(files.skeleton, joints, sizeof(joints));
    append(files.skeleton, inverseBind, sizeof(inverseBind));
    append(files.skeleton, bounds, sizeof(bounds));
    append(files.skeleton, skeletonArchive.data(), skeletonArchive.size());

    for (uint32_t trackCount : { 2u, 3u })
    {
        ozz::animation::offline::RawAnimation animation;
        animation.duration = 1.0f;
        animation.tracks.resize(trackCount);
        for (ozz::animation::offline::RawAnimation::JointTrack& track : animation.tracks)
        {
            track.translations.push_back({ 0.0f, ozz::math::Float3(0, 0, 0) });
            track.rotations.push_back({ 0.0f, ozz::math::Quaternion::identity() });
            track.scales.push_back({ 0.0f, ozz::math::Float3(1, 1, 1) });
        }
        animation.tracks[1].translations[0].value = ozz::math::Float3(0, 1, 0);
        animation.tracks[1].rotations.push_back({ 1.0f, ozz::math::Quaternion(0, 0, sqrtf(0.5f), sqrtf(0.5f)) });
        const ozz::unique_ptr<ozz::animation::Animation> built = ozz::animation::offline::AnimationBuilder()(animation);
        EXPECT_TRUE(built);
        const std::vector<char>       clipArchive = archive(*built);
        const hz::AnimationFileHeader clipHeader = { .archiveSize = (uint32_t)clipArchive.size() };
        std::vector<char>&            output = trackCount == 2 ? files.clip : files.mismatchedClip;
        append(output, &clipHeader, sizeof(clipHeader));
        append(output, clipArchive.data(), clipArchive.size());
    }
    return files;
}

Vector3 skin(const hz::SkinningCore& core, uint32_t joint, const Vector3& position)
{
    Matrix4 matrices[2];
    core.WriteSkinMatrices(0, matrices);
    return (matrices[joint] * Point3(position)).getXYZ();
}

// ozz stores rotation keys with 16-bit components, so sampled poses are accurate to about 1e-3.
constexpr float kPoseTolerance = 2e-3f;

void expectNear(const Vector3& actual, float x, float y, float z)
{
    EXPECT_NEAR(actual.getX(), x, kPoseTolerance);
    EXPECT_NEAR(actual.getY(), y, kPoseTolerance);
    EXPECT_NEAR(actual.getZ(), z, kPoseTolerance);
}

class AnimationTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(InitMemoryAlloc(nullptr));
        initialStats = GetMemoryTrackingStats();
        files = buildArm();
    }
    void TearDown() override
    {
        const MemoryTrackingStats stats = GetMemoryTrackingStats();
        if (stats.TrackingEnabled)
        {
            EXPECT_EQ(stats.LiveAllocationCount, initialStats.LiveAllocationCount);
            EXPECT_EQ(stats.LiveRequestedBytes, initialStats.LiveRequestedBytes);
        }
        ExitMemoryAlloc();
    }
    MemoryTrackingStats initialStats = {};
    ArmFiles            files;
};
} // namespace

TEST_F(AnimationTest, ParsesCookedFilesAndRejectsCorruptOnes)
{
    hz::SkeletonAsset skeleton;
    ASSERT_TRUE(skeleton.Parse({ files.skeleton.data(), (uint32_t)files.skeleton.size() }));
    EXPECT_EQ(skeleton.GetJointCount(), 2u);
    EXPECT_EQ(skeleton.GetSkinCount(), 1u);
    EXPECT_EQ(skeleton.GetSkinJointCount(0), 2u);
    hz::AnimationClipAsset clip;
    ASSERT_TRUE(clip.Parse({ files.clip.data(), (uint32_t)files.clip.size() }));
    EXPECT_FLOAT_EQ(clip.GetDuration(), 1.0f);
    EXPECT_EQ(clip.GetTrackCount(), 2u);

    // Truncated or retagged files fail without replacing the loaded data.
    std::vector<char> truncated(files.skeleton.begin(), files.skeleton.end() - 4);
    EXPECT_FALSE(skeleton.Parse({ truncated.data(), (uint32_t)truncated.size() }));
    std::vector<char> swapped = files.clip;
    memcpy(swapped.data(), files.skeleton.data(), 4);
    EXPECT_FALSE(clip.Parse({ swapped.data(), (uint32_t)swapped.size() }));
    std::vector<char> outOfRange = files.skeleton;
    const uint32_t    badJoint = 7;
    memcpy(outOfRange.data() + sizeof(hz::SkeletonFileHeader) + 2 * sizeof(uint32_t), &badJoint, sizeof(badJoint));
    EXPECT_FALSE(skeleton.Parse({ outOfRange.data(), (uint32_t)outOfRange.size() }));
    // Counts larger than the file fail before sizing any allocation.
    std::vector<char> oversized = files.skeleton;
    const uint32_t    hugeCount = UINT32_MAX;
    memcpy(oversized.data() + offsetof(hz::SkeletonFileHeader, skinCount), &hugeCount, sizeof(hugeCount));
    EXPECT_FALSE(skeleton.Parse({ oversized.data(), (uint32_t)oversized.size() }));
    oversized = files.skeleton;
    const uint32_t manyJoints = hz::kMaxSkinJoints;
    memcpy(oversized.data() + sizeof(hz::SkeletonFileHeader), &manyJoints, sizeof(manyJoints));
    EXPECT_FALSE(skeleton.Parse({ oversized.data(), (uint32_t)oversized.size() }));
    EXPECT_EQ(skeleton.GetJointCount(), 2u);
    EXPECT_EQ(clip.GetTrackCount(), 2u);
}

TEST_F(AnimationTest, SkinningCoreSamplesBlendsAndBoundsThePose)
{
    hz::SkeletonAsset      skeleton;
    hz::AnimationClipAsset clip, mismatched;
    ASSERT_TRUE(skeleton.Parse({ files.skeleton.data(), (uint32_t)files.skeleton.size() }));
    ASSERT_TRUE(clip.Parse({ files.clip.data(), (uint32_t)files.clip.size() }));
    ASSERT_TRUE(mismatched.Parse({ files.mismatchedClip.data(), (uint32_t)files.mismatchedClip.size() }));
    hz::SkinningCore core(skeleton);
    // The rest pose matches the bind pose, so skinning leaves bind-space vertices in place.
    expectNear(skin(core, 1, Vector3(1, 2, 0)), 1, 2, 0);
    EXPECT_EQ(core.GetModelMatrices().Count, 2u);

    const hz::AnimationLayer end = { .Clip = &clip, .Time = 1.0f };
    ASSERT_TRUE(core.Evaluate({ &end, 1 }));
    expectNear(skin(core, 1, Vector3(1, 2, 0)), -1, 2, 0);
    expectNear(skin(core, 0, Vector3(1, 0, 0)), 1, 0, 0);
    hz::SkinBounds bounds;
    ASSERT_TRUE(core.GetSkinBounds(0, bounds));
    EXPECT_NEAR(bounds.Minimum[0], -1, kPoseTolerance);
    EXPECT_NEAR(bounds.Maximum[0], 1, kPoseTolerance);
    EXPECT_NEAR(bounds.Minimum[1], 0, kPoseTolerance);
    EXPECT_NEAR(bounds.Maximum[1], 2, kPoseTolerance);

    const hz::AnimationLayer half = { .Clip = &clip, .Time = 0.5f };
    ASSERT_TRUE(core.Evaluate({ &half, 1 }));
    expectNear(skin(core, 1, Vector3(1, 2, 0)), 0, 1 + sqrtf(2.0f), 0);
    // Equal weights at the start and end of the clip blend to the same halfway pose.
    const hz::AnimationLayer layers[] = { { .Clip = &clip, .Time = 0.0f }, { .Clip = &clip, .Time = 1.0f } };
    ASSERT_TRUE(core.Evaluate(layers));
    expectNear(skin(core, 1, Vector3(1, 2, 0)), 0, 1 + sqrtf(2.0f), 0);

    // A clip for another skeleton fails and keeps the pose; no layers return to the rest pose.
    const hz::AnimationLayer foreign = { .Clip = &mismatched, .Time = 1.0f };
    EXPECT_FALSE(core.Evaluate({ &foreign, 1 }));
    expectNear(skin(core, 1, Vector3(1, 2, 0)), 0, 1 + sqrtf(2.0f), 0);
    ASSERT_TRUE(core.Evaluate({}));
    expectNear(skin(core, 1, Vector3(1, 2, 0)), 1, 2, 0);
}

namespace
{
struct MemoryLoader
{
    const ArmFiles* pFiles = nullptr;
    uint32_t        loads = 0;

    static void* begin(const hz::AssetCatalogEntry& entry, hz::AssetHandle, void* pUser)
    {
        MemoryLoader& loader = *(MemoryLoader*)pUser;
        ++loader.loads;
        if (entry.Type == hz::AssetType::Skeleton)
        {
            hz::SkeletonAsset* pAsset = MemNew(hz::SkeletonAsset);
            EXPECT_TRUE(pAsset->Parse({ (char*)loader.pFiles->skeleton.data(), (uint32_t)loader.pFiles->skeleton.size() }));
            return pAsset;
        }
        hz::AnimationClipAsset* pAsset = MemNew(hz::AnimationClipAsset);
        EXPECT_TRUE(pAsset->Parse({ (char*)loader.pFiles->clip.data(), (uint32_t)loader.pFiles->clip.size() }));
        return pAsset;
    }
    static hz::AssetLoadResult poll(void* pRequest, void*) { return { .status = hz::AssetLoadStatus::Succeeded, .pPayload = pRequest }; }
    static void                destroyRequest(void*, void*) {}
    static void                destroySkeleton(void* pPayload, void*) { MemDelete((hz::SkeletonAsset*)pPayload); }
    static void                destroyClip(void* pPayload, void*) { MemDelete((hz::AnimationClipAsset*)pPayload); }
};

const hz::AnimationPlayer& player(hz::World& world, hz::Entity entity)
{
    return *(const hz::AnimationPlayer*)world.GetComponent(entity, hz::SceneTypes::animationPlayer);
}
} // namespace

TEST_F(AnimationTest, AnimationSystemPlaysLoopsStopsAndReleasesAssets)
{
    const hz::AssetID skeletonID = { .high = 1, .low = 1 }, clipID = { .high = 1, .low = 2 };
    hz::AssetCatalog  catalog;
    ASSERT_TRUE(catalog.Add({ .id = skeletonID,
                              .Type = hz::AssetType::Skeleton,
                              .pSourcePath = "arm.gltf",
                              .location = { .pPath = "arm.skeleton0.ozz" } }));
    ASSERT_TRUE(catalog.Add({ .id = clipID,
                              .Type = hz::AssetType::AnimationClip,
                              .pSourcePath = "arm.gltf",
                              .location = { .pPath = "arm.animation0.ozz" },
                              .dependencies = { (hz::AssetID*)&skeletonID, 1 } }));
    MemoryLoader     loader = { .pFiles = &files };
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Skeleton, { .UserData = &loader,
                                                      .pBegin = MemoryLoader::begin,
                                                      .pPoll = MemoryLoader::poll,
                                                      .pDestroyRequest = MemoryLoader::destroyRequest,
                                                      .pDestroyPayload = MemoryLoader::destroySkeleton });
    assetManager.SetLoader(hz::AssetType::AnimationClip, { .UserData = &loader,
                                                           .pBegin = MemoryLoader::begin,
                                                           .pPoll = MemoryLoader::poll,
                                                           .pDestroyRequest = MemoryLoader::destroyRequest,
                                                           .pDestroyPayload = MemoryLoader::destroyClip });
    {
        hz::TypeRegistry types;
        ASSERT_TRUE(types.RegisterBuiltins());
        hz::World            world(types);
        hz::AnimationSystem  animation(world, assetManager);
        const hz::Entity     entity = world.CreatePersistentEntity();
        const hz::Skeleton   rig = { .asset = skeletonID };
        hz::AnimationPlayer  playing = { .clip = clipID };
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::skeleton, &rig));
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::animationPlayer, &playing));
        // Assets load across asset manager updates; until then the entity has no pose and its clock stands still.
        animation.Update(0.25f);
        EXPECT_EQ(animation.GetPose(entity), nullptr);
        EXPECT_EQ(player(world, entity).time, 0.0f);
        for (uint32_t i = 0; i < 8; ++i)
            assetManager.Update();
        animation.Update(0.25f);
        ASSERT_NE(animation.GetPose(entity), nullptr);
        EXPECT_FLOAT_EQ(player(world, entity).time, 0.25f);
        animation.Update(1.0f);
        EXPECT_FLOAT_EQ(player(world, entity).time, 0.25f);
        animation.Update(0.25f);
        EXPECT_FLOAT_EQ(player(world, entity).time, 0.5f);
        expectNear(skin(*animation.GetPose(entity), 1, Vector3(1, 2, 0)), 0, 1 + sqrtf(2.0f), 0);

        // A clip that does not loop stops at its end.
        playing = player(world, entity);
        playing.loop = false;
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::animationPlayer, &playing));
        animation.Update(2.0f);
        EXPECT_FLOAT_EQ(player(world, entity).time, 1.0f);
        EXPECT_FALSE(player(world, entity).playing);
        expectNear(skin(*animation.GetPose(entity), 1, Vector3(1, 2, 0)), -1, 2, 0);

        // Without a player the skeleton holds its rest pose; removing the skeleton releases its assets.
        ASSERT_TRUE(world.RemoveComponent(entity, hz::SceneTypes::animationPlayer));
        animation.Update(0.25f);
        expectNear(skin(*animation.GetPose(entity), 1, Vector3(1, 2, 0)), 1, 2, 0);
        ASSERT_TRUE(world.RemoveComponent(entity, hz::SceneTypes::skeleton));
        animation.Update(0.25f);
        EXPECT_EQ(animation.GetPose(entity), nullptr);
        assetManager.Update();
        EXPECT_EQ(assetManager.GetState(assetManager.Find(skeletonID)), hz::AssetState::Invalid);
    }
    EXPECT_EQ(loader.loads, 2u);
    assetManager.Shutdown();
}
