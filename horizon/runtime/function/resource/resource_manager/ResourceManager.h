/*****************************************************************//**
 * \file   ResourceManager.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <runtime/core/memory/Memory.h>

#include <runtime/function/rhi/RHI.h>
#include <runtime/function/resource/resources/mesh/Mesh.h>
#include <runtime/function/resource/resources/decal/Decal.h>
#include <runtime/function/resource/resources/vertex/VertexDescription.h>
#include <runtime/function/resource/resource_loader/mesh/MeshLoader.h>
#include <runtime/function/resource/resource_loader/texture/TextureLoader.h>

namespace Horizon {

class ResourceManager {
  public:
    ResourceManager(Backend::RHI *rhi, std::pmr::polymorphic_allocator<std::byte> allocator = {}) noexcept;
    ~ResourceManager() noexcept;

    Buffer *CreateGpuBuffer(const BufferCreateInfo &buffer_create_info, const Container::String &name = "");

    void DestroyGpuBuffer(Buffer *buffer);

    Texture *CreateGpuTexture(const TextureCreateInfo &texture_create_info, const Container::String &name = "");

    void DestroyGpuTexture(Texture *texture);

    Texture *GetEmptyTexture();

    Buffer *GetEmptyBuffer();

    Buffer *GetEmptyVertexBuffer();

    // uuid
    Mesh *LoadMesh(const MeshDesc &desc, const std::filesystem::path &path);

    void OffloadMesh(Mesh *mesh);

    Decal *LoadDecal(const std::filesystem::path &path);

    void OffloadDecal(Decal *decal);
    
    void ClearAllResources();

  public:
    Backend::RHI *m_rhi{};
    Container::HashSet<Buffer *> allocated_buffers;
    Buffer *empty_vertex_buffer;

    Container::HashSet<Texture *> allocated_textures;

    // Container::HashMap<u64, MeshFragmentResource> mesh_fragment_resources; // TODO(hylu): split mesh to meshfragment

    Container::HashSet<Mesh *> meshes;
    Container::HashSet<Decal *> decals;
    Container::HashSet<Material *> materials;
};

} // namespace Horizon
