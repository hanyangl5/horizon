#include "MeshLoader.h"

namespace Horizon {

Mesh *MeshLoader::Load(const MeshDesc &desc, const std::filesystem::path &path) {
    Mesh *mesh = new Mesh(desc, path);

    auto extension = path.extension();
    if (extension == ".gltf") {
        LoadGlTF2(*mesh);

    } else {
        LOG_ERROR("{} format is not supportted", extension.string().c_str());
    }
    return mesh;
}

void MeshLoader::LoadGlTF2(Mesh &mesh) { mesh.Load(); }

} // namespace Horizon