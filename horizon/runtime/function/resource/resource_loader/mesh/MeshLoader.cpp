#include "MeshLoader.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace Horizon {

Mesh *MeshLoader::Load(const MeshDesc &desc, const std::filesystem::path &path) {
    Mesh *mesh = new Mesh;

    auto &extension = path.extension();
    if (extension == ".gltf") {
        LoadGlTF2(desc, path, *mesh);

    } else {
        LOG_ERROR("{} format is not supportted", extension.string().c_str());
    }
    return mesh;
}

void MeshLoader::LoadGlTF2(const MeshDesc &desc, const std::filesystem::path &path, Mesh &mesh) {

    // ReadFile();

    cgltf_options options = {};
    cgltf_data *data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path.string().c_str(), &data);
    if (result != cgltf_result_success) {
        LOG_ERROR("failed to load gltf mesh");
        return;
    }
    if (data->file_type == cgltf_file_type::cgltf_file_type_gltf) {
    

    }
    cgltf_load_buffers(&options, data, path.string().c_str());
    cgltf_validate(data);
    //cgltf_load_buffer_base64();
    std::vector<Vertex> vertices; 
    for (u32 i = 0; i < data->buffers_count; i++) {
        //auto buffer_path = path.parent_path() / data->buffers[i].uri;
        //auto vec = ReadFile(buffer_path.string().c_str());

    }
    for (u32 i = 0; i < data->images_count; i++) {
        data->images[i].uri;

    }

    LOG_INFO("{}", data->images->uri);
    cgltf_free(data);
}

void MeshLoader::LoadFBX(const MeshDesc &desc, const std::filesystem::path &path, Mesh &mesh) {
    
}

} // namespace Horizon