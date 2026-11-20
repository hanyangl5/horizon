/* Copyright (c) 2026 Horizon */

#include "SceneSkeletonCooker.h"
#include "CookUtilities.h"
#include "Core/ILog.h"
#include "Core/IMath.h"
#include "Runtime/Scene/Private/Animation/AnimationFiles.h"
#include <ThirdParty/cgltf/cgltf.h>
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/animation_optimizer.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Core/IMemory.h"

using ozz::animation::offline::RawAnimation;
using ozz::animation::offline::RawSkeleton;

static uint32_t nodeIndex(const cgltf_data& data, const cgltf_node* pNode) { return (uint32_t)(pNode - data.nodes); }

static int compareIndices(const void* pA, const void* pB)
{
    const uint32_t a = *(const uint32_t*)pA;
    const uint32_t b = *(const uint32_t*)pB;
    return (a > b) - (a < b);
}

static void append(hz::Array<char>& bytes, const void* pData, size_t size)
{
    const uint32_t offset = bytes.Size();
    bytes.Resize(offset + (uint32_t)size);
    memcpy(bytes.GetData() + offset, pData, size);
}

template<typename T>
static bool archive(const T& object, hz::Array<char>& bytes)
{
    ozz::io::MemoryStream stream;
    {
        ozz::io::OArchive output(&stream);
        output << object;
    }
    const size_t size = stream.Size();
    bytes.Resize((uint32_t)size);
    stream.Seek(0, ozz::io::Stream::kSet);
    return stream.Read(bytes.GetData(), size) == size;
}

// Skeleton nodes may store a matrix when they are not animated; ozz needs the decomposed rest pose.
static ozz::math::Transform restTransform(const cgltf_node& node)
{
    ozz::math::Transform transform = ozz::math::Transform::identity();
    if (!node.has_matrix)
    {
        transform.translation = ozz::math::Float3(node.translation[0], node.translation[1], node.translation[2]);
        transform.rotation = ozz::math::Quaternion(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
        transform.scale = ozz::math::Float3(node.scale[0], node.scale[1], node.scale[2]);
        return transform;
    }
    const float*  m = node.matrix;
    const Vector3 x(m[0], m[1], m[2]), y(m[4], m[5], m[6]), z(m[8], m[9], m[10]);
    float         scaleX = length(x);
    const float   scaleY = length(y), scaleZ = length(z);
    if (dot(cross(x, y), z) < 0.0f)
        scaleX = -scaleX;
    const Quat rotation = normalize(Quat(Matrix3(x / scaleX, y / scaleY, z / scaleZ)));
    transform.translation = ozz::math::Float3(m[12], m[13], m[14]);
    transform.rotation = ozz::math::Quaternion(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW());
    transform.scale = ozz::math::Float3(scaleX, scaleY, scaleZ);
    return transform;
}

static void addJoint(const cgltf_data& data, uint32_t node, const hz::Array<uint8_t>& members, RawSkeleton::Joint& joint)
{
    char name[32];
    snprintf(name, sizeof(name), "node%u", node);
    joint.name = name;
    joint.transform = restTransform(data.nodes[node]);
    const cgltf_node& source = data.nodes[node];
    for (cgltf_size i = 0; i < source.children_count; ++i)
    {
        const uint32_t child = nodeIndex(data, source.children[i]);
        if (members[child])
        {
            joint.children.emplace_back();
            addJoint(data, child, members, joint.children.back());
        }
    }
}

static const cgltf_accessor* findAttribute(const cgltf_primitive& primitive, cgltf_attribute_type type)
{
    for (cgltf_size i = 0; i < primitive.attributes_count; ++i)
        if (primitive.attributes[i].type == type && primitive.attributes[i].index == 0)
            return primitive.attributes[i].data;
    return nullptr;
}

// Bounds of the vertices each joint influences, in the joint's bind space, so the runtime can bound an animated pose
// by transforming one box per joint.
static bool buildJointBounds(const cgltf_data& data, const cgltf_skin& skin, hz::Span<Matrix4> inverseBind, hz::Array<float>& bounds)
{
    const uint32_t jointCount = (uint32_t)skin.joints_count;
    bounds.Resize(jointCount * 6);
    for (uint32_t joint = 0; joint < jointCount; ++joint)
        for (uint32_t axis = 0; axis < 3; ++axis)
        {
            bounds[joint * 6 + axis] = FLT_MAX;
            bounds[joint * 6 + 3 + axis] = -FLT_MAX;
        }
    for (cgltf_size n = 0; n < data.nodes_count; ++n)
    {
        if (data.nodes[n].skin != &skin || !data.nodes[n].mesh)
            continue;
        const cgltf_mesh& mesh = *data.nodes[n].mesh;
        for (cgltf_size p = 0; p < mesh.primitives_count; ++p)
        {
            const cgltf_accessor* pPositions = findAttribute(mesh.primitives[p], cgltf_attribute_type_position);
            const cgltf_accessor* pJoints = findAttribute(mesh.primitives[p], cgltf_attribute_type_joints);
            const cgltf_accessor* pWeights = findAttribute(mesh.primitives[p], cgltf_attribute_type_weights);
            if (!pPositions || !pJoints || !pWeights || pJoints->count != pPositions->count || pWeights->count != pPositions->count)
            {
                LOGF(Error, "Skinned mesh '%s' primitive %u needs POSITION, JOINTS_0 and WEIGHTS_0 of equal length",
                     mesh.name ? mesh.name : "", (uint32_t)p);
                return false;
            }
            for (cgltf_size v = 0; v < pPositions->count; ++v)
            {
                float position[3], joints[4], weights[4];
                if (!cgltf_accessor_read_float(pPositions, v, position, 3) || !cgltf_accessor_read_float(pJoints, v, joints, 4) ||
                    !cgltf_accessor_read_float(pWeights, v, weights, 4))
                    return false;
                for (uint32_t k = 0; k < 4; ++k)
                {
                    if (weights[k] <= 0.0f)
                        continue;
                    const uint32_t joint = (uint32_t)joints[k];
                    if (joint >= jointCount)
                    {
                        LOGF(Error, "Skinned mesh '%s' references joint %u of a %u-joint skin", mesh.name ? mesh.name : "", joint, jointCount);
                        return false;
                    }
                    const Vector4 local = inverseBind.Data[joint] * Point3(position[0], position[1], position[2]);
                    float*        pBounds = bounds.GetData() + joint * 6;
                    for (uint32_t axis = 0; axis < 3; ++axis)
                    {
                        pBounds[axis] = fminf(pBounds[axis], local.getElem(axis));
                        pBounds[3 + axis] = fmaxf(pBounds[3 + axis], local.getElem(axis));
                    }
                }
            }
        }
    }
    return true;
}

// Reads one animation sampler into times and values. Cubic splines keep their key values without tangents, and step
// samplers repeat each value just before the next key, as ozz interpolates every track linearly.
static bool readSampler(const cgltf_animation_sampler& sampler, uint32_t components, hz::Array<float>& times, hz::Array<float>& values)
{
    const bool     cubic = sampler.interpolation == cgltf_interpolation_type_cubic_spline;
    const bool     step = sampler.interpolation == cgltf_interpolation_type_step;
    const uint32_t count = (uint32_t)sampler.input->count;
    if (!count || sampler.output->count != count * (cubic ? 3u : 1u))
        return false;
    times.Clear();
    values.Clear();
    for (uint32_t i = 0; i < count; ++i)
    {
        float time;
        float value[4];
        if (!cgltf_accessor_read_float(sampler.input, i, &time, 1) ||
            !cgltf_accessor_read_float(sampler.output, cubic ? i * 3 + 1 : i, value, components) || time < 0.0f ||
            (!times.Empty() && time <= times[times.Size() - 1]))
            return false;
        if (step && i)
        {
            const float previous = times[times.Size() - 1];
            const float hold = time - fminf(1e-4f, (time - previous) * 0.5f);
            if (hold > previous)
            {
                times.PushBack(hold);
                for (uint32_t c = 0; c < components; ++c)
                    values.PushBack(values[values.Size() - components]);
            }
        }
        times.PushBack(time);
        for (uint32_t c = 0; c < components; ++c)
            values.PushBack(value[c]);
    }
    return true;
}

uint32_t SceneSkeletonCooker::findSkeleton(uint32_t skin, uint32_t& skinIndex) const
{
    for (uint32_t i = 0; i < skeletons.Size(); ++i)
        for (uint32_t j = 0; j < skeletons[i].skins.Size(); ++j)
            if (skeletons[i].skins[j] == skin)
            {
                skinIndex = j;
                return i;
            }
    return UINT32_MAX;
}

bool SceneSkeletonCooker::cook(const cgltf_data& data, const char* pBaseName)
{
    const uint32_t nodeCount = (uint32_t)data.nodes_count;
    skeletonNodes.Resize(nodeCount);
    memset(skeletonNodes.GetData(), 0, skeletonNodes.Size());
    // Per cooked skeleton, a flag per glTF node that belongs to it.
    hz::Array<hz::Array<uint8_t>> members;
    hz::Array<uint8_t>            joints(nodeCount);
    hz::Array<uint32_t>           roots;
    for (uint32_t s = 0; s < (uint32_t)data.skins_count; ++s)
    {
        const cgltf_skin& skin = data.skins[s];
        if (!skin.joints_count || skin.joints_count > hz::kMaxSkinJoints)
        {
            LOGF(Error, "Skin %u has %u joints; skins need 1 to %u joints", s, (uint32_t)skin.joints_count, hz::kMaxSkinJoints);
            return false;
        }
        memset(joints.GetData(), 0, joints.Size());
        for (cgltf_size j = 0; j < skin.joints_count; ++j)
            joints[nodeIndex(data, skin.joints[j])] = 1;
        roots.Clear();
        if (skin.skeleton)
            roots.PushBack(nodeIndex(data, skin.skeleton));
        else
            for (cgltf_size j = 0; j < skin.joints_count; ++j)
            {
                const cgltf_node* pParent = skin.joints[j]->parent;
                if (!pParent || !joints[nodeIndex(data, pParent)])
                    roots.PushBack(nodeIndex(data, skin.joints[j]));
            }
        qsort(roots.GetData(), roots.Size(), sizeof(uint32_t), compareIndices);
        const cgltf_node* pParent = data.nodes[roots[0]].parent;
        for (uint32_t root : roots)
            if (data.nodes[root].parent != pParent)
            {
                LOGF(Error, "Skin %u has root joints below different parents", s);
                return false;
            }
        uint32_t skeleton = 0;
        while (skeleton < skeletons.Size() &&
               (skeletons[skeleton].roots.Size() != roots.Size() ||
                memcmp(skeletons[skeleton].roots.GetData(), roots.GetData(), roots.Size() * sizeof(uint32_t))))
            ++skeleton;
        if (skeleton == skeletons.Size())
        {
            CookedSkeleton& cooked = skeletons.EmplaceBack();
            cooked.roots = roots;
            cooked.parent = pParent ? nodeIndex(data, pParent) : UINT32_MAX;
            members.EmplaceBack(nodeCount);
            memset(members[skeleton].GetData(), 0, nodeCount);
            for (uint32_t root : roots)
                members[skeleton][root] = 1;
        }
        skeletons[skeleton].skins.PushBack(s);
        // Joints and the nodes between them and the roots form the skeleton.
        for (cgltf_size j = 0; j < skin.joints_count; ++j)
        {
            const cgltf_node* pNode = skin.joints[j];
            while (!members[skeleton][nodeIndex(data, pNode)])
            {
                members[skeleton][nodeIndex(data, pNode)] = 1;
                pNode = pNode->parent;
                if (!pNode)
                {
                    LOGF(Error, "Skin %u joint '%s' is not below the skin's skeleton root", s,
                         skin.joints[j]->name ? skin.joints[j]->name : "");
                    return false;
                }
            }
        }
    }

    hz::Array<ozz::unique_ptr<ozz::animation::Skeleton>> built;
    hz::Array<hz::Array<uint32_t>>                       jointNodes;
    for (uint32_t k = 0; k < skeletons.Size(); ++k)
    {
        CookedSkeleton& cooked = skeletons[k];
        for (uint32_t n = 0; n < nodeCount; ++n)
        {
            if (!members[k][n])
                continue;
            if (skeletonNodes[n])
            {
                LOGF(Error, "Skins with overlapping skeleton hierarchies are not supported (node %u)", n);
                return false;
            }
            skeletonNodes[n] = 1;
        }
        RawSkeleton raw;
        for (uint32_t root : cooked.roots)
        {
            raw.roots.emplace_back();
            addJoint(data, root, members[k], raw.roots.back());
        }
        ozz::animation::offline::SkeletonBuilder builder;
        built.PushBack(builder(raw));
        if (!built[k])
        {
            LOGF(Error, "Cannot build skeleton %u", k);
            return false;
        }
        const ozz::animation::Skeleton& skeleton = *built[k];
        hz::Array<uint32_t>&            nodes = jointNodes.EmplaceBack((uint32_t)skeleton.num_joints());
        for (int j = 0; j < skeleton.num_joints(); ++j)
            nodes[(uint32_t)j] = (uint32_t)atoi(skeleton.joint_names()[j] + 4);
        cooked.signature = cookHash(cooked.roots.GetData(), cooked.roots.Size() * sizeof(uint32_t));
        cooked.signature = cookHash(nodes.GetData(), nodes.Size() * sizeof(uint32_t), cooked.signature);
        const char* pName = data.nodes[cooked.roots[0]].name;
        snprintf(cooked.name, sizeof(cooked.name), "%s", pName && *pName ? pName : "Skeleton");
        snprintf(cooked.path, sizeof(cooked.path), "%s.skeleton%u.ozz", pBaseName, k);

        hz::Array<char> ozzBytes;
        if (!archive(skeleton, ozzBytes))
            return false;
        const hz::SkeletonFileHeader header = { .skinCount = cooked.skins.Size(), .archiveSize = ozzBytes.Size() };
        append(cooked.file, &header, sizeof(header));
        for (uint32_t skinIndex : cooked.skins)
        {
            const cgltf_skin& skin = data.skins[skinIndex];
            const uint32_t    jointCount = (uint32_t)skin.joints_count;
            hz::Array<uint32_t> remap(jointCount);
            hz::Array<Matrix4>  inverseBind(jointCount);
            for (uint32_t j = 0; j < jointCount; ++j)
            {
                const uint32_t node = nodeIndex(data, skin.joints[j]);
                remap[j] = UINT32_MAX;
                for (uint32_t i = 0; i < nodes.Size(); ++i)
                    if (nodes[i] == node)
                        remap[j] = i;
                float values[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
                if (skin.inverse_bind_matrices && !cgltf_accessor_read_float(skin.inverse_bind_matrices, j, values, 16))
                    return false;
                inverseBind[j] = Matrix4(Vector4(values[0], values[1], values[2], values[3]), Vector4(values[4], values[5], values[6], values[7]),
                                         Vector4(values[8], values[9], values[10], values[11]),
                                         Vector4(values[12], values[13], values[14], values[15]));
            }
            hz::Array<float> bounds;
            if (!buildJointBounds(data, skin, { inverseBind.GetData(), inverseBind.Size() }, bounds))
                return false;
            append(cooked.file, &jointCount, sizeof(jointCount));
            append(cooked.file, remap.GetData(), remap.Size() * sizeof(uint32_t));
            for (const Matrix4& matrix : inverseBind)
                for (uint32_t column = 0; column < 4; ++column)
                    for (uint32_t row = 0; row < 4; ++row)
                    {
                        const float value = matrix.getElem((int)column, (int)row);
                        append(cooked.file, &value, sizeof(value));
                    }
            append(cooked.file, bounds.GetData(), bounds.Size() * sizeof(float));
        }
        append(cooked.file, ozzBytes.GetData(), ozzBytes.Size());
    }

    hz::Array<float> times, values;
    for (uint32_t a = 0; a < (uint32_t)data.animations_count; ++a)
    {
        const cgltf_animation& animation = data.animations[a];
        float                  duration = 0.0f;
        uint32_t               ignored = 0;
        for (cgltf_size c = 0; c < animation.channels_count; ++c)
        {
            const cgltf_animation_channel& channel = animation.channels[c];
            const cgltf_accessor&          input = *channel.sampler->input;
            float                          last = 0.0f;
            if (input.count && cgltf_accessor_read_float(&input, input.count - 1, &last, 1))
                duration = fmaxf(duration, last);
            if (channel.target_path == cgltf_animation_path_type_weights || !channel.target_node ||
                !skeletonNodes[nodeIndex(data, channel.target_node)])
                ++ignored;
        }
        if (ignored)
            LOGF(Warning, "Animation %u ignores %u channels: only skeleton joint transforms are animated", a, ignored);
        for (uint32_t k = 0; k < skeletons.Size(); ++k)
        {
            const hz::Array<uint32_t>& nodes = jointNodes[k];
            RawAnimation               raw;
            raw.duration = fmaxf(duration, 1.0f / 30.0f);
            raw.tracks.resize(nodes.Size());
            uint64_t signature = cookHash(&k, sizeof(k));
            uint32_t channels = 0;
            for (uint32_t j = 0; j < nodes.Size(); ++j)
            {
                RawAnimation::JointTrack& track = raw.tracks[j];
                for (cgltf_size c = 0; c < animation.channels_count; ++c)
                {
                    const cgltf_animation_channel& channel = animation.channels[c];
                    if (!channel.target_node || nodeIndex(data, channel.target_node) != nodes[j] ||
                        channel.target_path == cgltf_animation_path_type_weights)
                        continue;
                    const uint32_t components = channel.target_path == cgltf_animation_path_type_rotation ? 4 : 3;
                    if (!readSampler(*channel.sampler, components, times, values))
                    {
                        LOGF(Error, "Animation %u channel %u has invalid keys", a, (uint32_t)c);
                        return false;
                    }
                    ++channels;
                    signature = cookHash(&nodes[j], sizeof(nodes[j]), signature);
                    signature = cookHash(&channel.target_path, sizeof(channel.target_path), signature);
                    signature = cookHash(times.GetData(), times.Size() * sizeof(float), signature);
                    signature = cookHash(values.GetData(), values.Size() * sizeof(float), signature);
                    for (uint32_t i = 0; i < times.Size(); ++i)
                    {
                        const float* pValue = values.GetData() + i * components;
                        if (channel.target_path == cgltf_animation_path_type_translation)
                            track.translations.push_back({ times[i], ozz::math::Float3(pValue[0], pValue[1], pValue[2]) });
                        else if (channel.target_path == cgltf_animation_path_type_scale)
                            track.scales.push_back({ times[i], ozz::math::Float3(pValue[0], pValue[1], pValue[2]) });
                        else
                            track.rotations.push_back(
                                { times[i], ozz::math::Normalize(ozz::math::Quaternion(pValue[0], pValue[1], pValue[2], pValue[3])) });
                    }
                }
                // Paths without a channel hold the rest pose.
                const ozz::math::Transform rest = restTransform(data.nodes[nodes[j]]);
                if (track.translations.empty())
                    track.translations.push_back({ 0.0f, rest.translation });
                if (track.rotations.empty())
                    track.rotations.push_back({ 0.0f, rest.rotation });
                if (track.scales.empty())
                    track.scales.push_back({ 0.0f, rest.scale });
            }
            if (!channels)
                continue;
            RawAnimation                              optimized;
            ozz::animation::offline::AnimationOptimizer optimizer;
            ozz::animation::offline::AnimationBuilder   builder;
            if (!raw.Validate() || !optimizer(raw, *built[k], &optimized))
            {
                LOGF(Error, "Animation %u has keys outside its duration or out of order", a);
                return false;
            }
            const ozz::unique_ptr<ozz::animation::Animation> clip = builder(optimized);
            hz::Array<char>                                  ozzBytes;
            if (!clip || !archive(*clip, ozzBytes))
                return false;
            CookedAnimation& cooked = animations.EmplaceBack();
            cooked.animation = a;
            cooked.skeleton = k;
            cooked.signature = signature;
            if (animation.name && *animation.name)
                snprintf(cooked.name, sizeof(cooked.name), "%s", animation.name);
            else
                snprintf(cooked.name, sizeof(cooked.name), "Animation %u", a);
            snprintf(cooked.path, sizeof(cooked.path), "%s.animation%u.ozz", pBaseName, animations.Size() - 1);
            const hz::AnimationFileHeader header = { .archiveSize = ozzBytes.Size() };
            append(cooked.file, &header, sizeof(header));
            append(cooked.file, ozzBytes.GetData(), ozzBytes.Size());
            LOGF(Info, "Animation '%s': %u joints, %.2f s, %u bytes", cooked.name, nodes.Size(), raw.duration, cooked.file.Size());
        }
    }
    return true;
}
