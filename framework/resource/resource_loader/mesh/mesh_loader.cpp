#include "mesh_loader.h"

namespace Horizon
{

Mesh *MeshLoader::Load(const MeshDesc &desc, const char *path)
{
    const char *path_obj(path);
    Mesh *mesh = new Mesh(desc, path_obj);
    mesh->Load();
    return mesh;
}

} // namespace Horizon
