/*****************************************************************/ /**
 * \file   ResourceManager.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <unordered_set>
#include <vector>

#include <runtime/core/memory/Memory.h>

#include <runtime/resource/resource_loader/mesh/MeshLoader.h>
#include <runtime/resource/resource_loader/texture/TextureLoader.h>
#include <runtime/resource/resources/mesh/Mesh.h>
#include <runtime/resource/resources/vertex/VertexDescription.h>
#include <runtime/rhi/RHI.h>

namespace Horizon {

extern std::array<Vertex, 36> cube_vertices;
extern std::array<Index, 36> cube_indices;

class ResourceManager {
  public:
    ResourceManager(Backend::RHI *rhi) noexcept;
    ~ResourceManager() noexcept;

    Buffer *CreateGpuBuffer(const BufferCreateInfo &buffer_create_info);

    void DestroyGpuBuffer(Buffer *buffer);

    Texture *CreateGpuTexture(const TextureCreateInfo &texture_create_info);

    void DestroyGpuTexture(Texture *texture);

    Texture *GetEmptyTexture();

    Buffer *GetEmptyBuffer();

    Buffer *GetEmptyVertexBuffer();

    // uuid
    Mesh *LoadMesh(const MeshDesc &desc, const std::filesystem::path &path);

    void OffloadMesh(Mesh *mesh);

    void ClearAllResources();

  public:
    Backend::RHI *mRhi{};
    std::unordered_set<Buffer *> allocated_buffers;
    Buffer *empty_vertex_buffer;

    std::unordered_set<Texture *> allocated_textures;

    std::unordered_set<Mesh *> meshes;
    std::unordered_set<Material *> materials;
};

} // namespace Horizon
