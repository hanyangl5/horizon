#include "mesh_loader.h"

#include <core/log.h>
#include <core/path.h>

namespace Horizon
{

Mesh *MeshLoader::Load(const MeshDesc &desc, const char *path)
{
    const char *path_obj(path);
    Mesh *mesh = new Mesh(desc, path_obj);

    if (desc.mesh_format == EMeshAssetFormat::MESH_FORMAT_GLTF)
    {
        LoadGlTF2(*mesh);
    }
    else
    {
        LOG_ERROR("{} format is not supportted", (u32)desc.mesh_format);
    }
    return mesh;
}

void MeshLoader::LoadGlTF2(Mesh &mesh)
{
    mesh.Load();
}

} // namespace Horizon