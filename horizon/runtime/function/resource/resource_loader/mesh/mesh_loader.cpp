/*****************************************************************//**
 * \file   mesh_loader.cpp
 * \brief  
 * 
 * \author hylu
 * \date   January 2023
 *********************************************************************/

#include "mesh_loader.h"

// standard libraries

// third party libraries

// project headers
#include <runtime/core/memory/memory.h>

// TODO: replace assimp with fbxsdk and cgltf

namespace Horizon {

Mesh *MeshLoader::Load(const MeshDesc &desc, const std::filesystem::path &path) {
    Mesh *mesh = Memory::Alloc<Mesh>(desc);

    auto extension = path.extension();
    if (extension == ".gltf") {
        LoadGlTF2(path, *mesh);
    } else if (extension == ".fbx") {
        LoadFBX(path, *mesh);
    } else {
        LOG_ERROR("{} format is not supportted", extension.string().c_str());
    }
    return mesh;
}

void MeshLoader::LoadGlTF2(const std::filesystem::path &path, Mesh &mesh) { mesh.Load(path); }

void MeshLoader::LoadFBX(const std::filesystem::path &path, Mesh &mesh) { mesh.Load(path); }

} // namespace Horizon