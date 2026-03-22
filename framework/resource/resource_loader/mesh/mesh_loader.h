// load scene from gltf/fbx file
#pragma once

#include <core/path.h>

#include <core/definations.h>
#include <resource/resources/mesh/mesh.h>
#include <rhi/enums.h>

namespace Horizon
{

class MeshLoader
{
  public:
    static Mesh *Load(const MeshDesc &desc, const char *path);
};
} // namespace Horizon
