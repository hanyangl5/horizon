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

// TODO: replace assimp with fbxsdk and cgltf

namespace Horizon {

Mesh *MeshLoader::Load(const MeshDesc &desc, const std::filesystem::path &path) {
    Mesh *mesh = new Mesh(desc, path);

    auto &extension = path.extension();
    if (extension == ".gltf") {
        LoadGlTF2(desc, path, *mesh);
    } else if (extension == ".fbx") {
        LoadFBX(desc, path, *mesh);
    } else {
        LOG_ERROR("{} format is not supportted", extension.string().c_str());
    }
    return mesh;
}

void MeshLoader::LoadGlTF2(const MeshDesc &desc, const std::filesystem::path &path, Mesh &mesh) { mesh.Load(); }

void MeshLoader::LoadFBX(const MeshDesc &desc, const std::filesystem::path &path, Mesh &mesh) { mesh.Load(); }

} // namespace Horizon