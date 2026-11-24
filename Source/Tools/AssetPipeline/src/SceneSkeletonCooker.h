/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IContainer.h"
#include "Core/IFileSystem.h"

struct cgltf_data;

// One ozz skeleton, built from the glTF skins whose joints hang below the same root nodes.
struct CookedSkeleton
{
    // glTF node indices of the root nodes; their common parent is the node the skeleton entity attaches to.
    hz::Array<uint32_t> roots;
    uint32_t            parent = UINT32_MAX;
    // glTF skin indices, in the order of the skins in the cooked file.
    hz::Array<uint32_t> skins;
    uint64_t            signature = 0;
    char                name[128] = {};
    char                path[FS_MAX_PATH] = {};
    hz::Array<char>     file;
};

// One glTF animation restricted to the joints of one skeleton.
struct CookedAnimation
{
    uint32_t        animation = 0;
    uint32_t        skeleton = 0;
    uint64_t        signature = 0;
    char            name[128] = {};
    char            path[FS_MAX_PATH] = {};
    hz::Array<char> file;
};

// Converts glTF skins and animations to ozz skeletons and clips. Skeleton nodes leave the World hierarchy: the
// skeleton entity's animation drives them, and the skinned meshes attach below that entity.
class SceneSkeletonCooker
{
public:
    // Cooked files are named <pBaseName>.skeleton<i>.ozz and <pBaseName>.animation<i>.ozz.
    bool     cook(const cgltf_data& data, const char* pBaseName);
    bool     isSkeletonNode(uint32_t node) const { return node < skeletonNodes.Size() && skeletonNodes[node]; }
    // Returns the cooked skeleton of a glTF skin and the skin's index within it.
    uint32_t findSkeleton(uint32_t skin, uint32_t& skinIndex) const;

    hz::Array<CookedSkeleton>  skeletons;
    hz::Array<CookedAnimation> animations;

private:
    hz::Array<uint8_t> skeletonNodes;
};
