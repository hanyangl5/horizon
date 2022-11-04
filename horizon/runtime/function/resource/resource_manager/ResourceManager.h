#pragma once

#include <runtime/core/memory/Alloc.h>

#include <runtime/function/rhi/RHI.h>
#include <runtime/function/resource/resources/mesh/Mesh.h>
#include <runtime/function/resource/resources/vertex/VertexDescription.h>
#include <runtime/function/resource/resource_loader/mesh/MeshLoader.h>
#include <runtime/function/resource/resource_loader/texture/TextureLoader.h>

namespace Horizon {

class ResourceManager {
  public:
    ResourceManager(Backend::RHI *rhi) noexcept;
    ~ResourceManager() noexcept;

    Buffer *CreateGpuBuffer(const BufferCreateInfo &buffer_create_info, const Container::String &name = "");

    void DestroyGpuBuffer(Buffer *buffer);

    Texture *CreateGpuTexture(const TextureCreateInfo &texture_create_info, const Container::String &name = "");

    void DestroyGpuTexture(Texture *texture);

    Texture *GetEmptyTexture();
    Buffer *GetEmptyBuffer();
    Buffer *GetEmptyVertexBuffer();

    // uuid
    Mesh *LoadMesh(const MeshDesc &desc, std::filesystem::path path);
    void OffloadMesh(Mesh *mesh);

    void ClearAllResources();

  public:
    Backend::RHI *m_rhi{};
    std::unordered_set<Buffer *> allocated_buffers;
    Buffer *empty_vertex_buffer;

    std::unordered_set<Texture *> allocated_textures;

    // Container::HashMap<u64, MeshFragmentResource> mesh_fragment_resources; //TODO

    std::unordered_set<Mesh *> meshes;
    std::unordered_set<Material *> materials;
};

} // namespace Horizon
