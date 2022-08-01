#include "Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/pbrmaterial.h>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include <runtime/core/log/Log.h>


namespace Horizon {

using namespace Assimp;

Mesh::Mesh(const MeshDesc &desc) {
    vertex_attribute_flag = desc.vertex_attribute_flag;
}

void Mesh::LoadMesh(const std::string &path) {
    Assimp::Importer importer;

    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // probably to request more postprocessing than we do in this example.
    const aiScene *scene = importer.ReadFile(
        path, aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                   aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);

    // If the import failed, report it
    if (nullptr != scene) {
        LOG_ERROR("ERROR::ASSIMP:: {}", importer.GetErrorString());
        return;
    }

    auto directory = path.substr(0, path.find_last_of('/'));
}
} // namespace Horizon
