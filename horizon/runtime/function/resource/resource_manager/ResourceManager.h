#pragma once

#include <unordered_map>
#include <unordered_set>

#include <runtime/function/resource/resources/mesh/Mesh.h>
#include <runtime/function/resource/resources/vertex/VertexDescription.h>

#include <runtime/function/rhi/RHI.h>

namespace Horizon {

class ResourceManager {
  public:
    ResourceManager(Backend::RHI *rhi) noexcept;
    ~ResourceManager() noexcept;

    Buffer *CreateGpuBuffer(const BufferCreateInfo &buffer_create_info, const std::string &name = "");

    void DestroyGpuBuffer(Buffer *buffer);

    Texture *CreateGpuTexture(const TextureCreateInfo &texture_create_info, const std::string &name = "");

    void DestroyGpuTexture(Texture *texture);

    Texture *GetEmptyTexture();
    Buffer *GetEmptyBuffer();
    Buffer *GetEmptyVertexBuffer();

  public:
    Backend::RHI *m_rhi{};
    std::unordered_set<Buffer *> allocated_buffers;
    Buffer *empty_vertex_buffer;

    std::unordered_set<Texture *> allocated_textures;

    std::unordered_map<u64, MeshFragmentResource> mesh_fragment_resources;
    std::unordered_map<u64, Material> materials;
};

/*
small vertex buffer:
better instancing, better 
compact to big vertex buffer:
*/

} // namespace Horizon
