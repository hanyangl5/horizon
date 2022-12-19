#include "ResourceManager.h"

namespace Horizon {
ResourceManager::ResourceManager(Backend::RHI *rhi, std::pmr::polymorphic_allocator<std::byte> allocator) noexcept
    : m_rhi(rhi) {}

ResourceManager::~ResourceManager() noexcept { ClearAllResources(); }

Buffer *ResourceManager::CreateGpuBuffer(const BufferCreateInfo &buffer_create_info, const Container::String &name) {
    auto buffer = m_rhi->CreateBuffer(buffer_create_info);
    allocated_buffers.emplace(buffer);
    return buffer;
}

Buffer *ResourceManager::GetEmptyVertexBuffer() {

    if (empty_vertex_buffer == nullptr) {

        BufferCreateInfo vertex_buffer_create_info{};
        vertex_buffer_create_info.size = 1;
        vertex_buffer_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_VERTEX_BUFFER;
        vertex_buffer_create_info.initial_state = ResourceState::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        empty_vertex_buffer = m_rhi->CreateBuffer(vertex_buffer_create_info);
    }
    return empty_vertex_buffer;
}

Mesh *ResourceManager::LoadMesh(const MeshDesc &desc, const std::filesystem::path& path) {
    auto mesh = MeshLoader::Load(desc, path);
    meshes.emplace(mesh);
    return mesh;
}

Decal *ResourceManager::LoadDecal(const std::filesystem::path &path) {
    Decal *decal = Memory::Alloc<Decal>(path);
    decals.emplace(decal);
    return decal;
}

void ResourceManager::OffloadDecal(Decal *decal) {}

void ResourceManager::OffloadMesh(Mesh *mesh) {
    if (meshes.find(mesh) != meshes.end()) {
        meshes.erase(mesh);
        Memory::Free(mesh);
        mesh = nullptr;
    }
}

void ResourceManager::ClearAllResources() {
    for (auto &remain_buffer : allocated_buffers) {
        if (remain_buffer != nullptr) {
            allocated_buffers.erase(remain_buffer);
            m_rhi->DestroyBuffer(remain_buffer);
        }
    }
    for (auto &remain_texture : allocated_textures) {
        if (remain_texture != nullptr) {
            allocated_textures.erase(remain_texture);
            m_rhi->DestroyTexture(remain_texture);
        }
    }
}

void ResourceManager::DestroyGpuBuffer(Buffer *buffer) {
    if (allocated_buffers.find(buffer) != allocated_buffers.end()) {
        allocated_buffers.erase(buffer);
        m_rhi->DestroyBuffer(buffer);
    }
}

Texture *ResourceManager::CreateGpuTexture(const TextureCreateInfo &texture_create_info,
                                           const Container::String &name) {
    auto texture = m_rhi->CreateTexture(texture_create_info);
    allocated_textures.emplace(texture);
    return texture;
}

void ResourceManager::DestroyGpuTexture(Texture *texture) {
    if (allocated_textures.find(texture) != allocated_textures.end()) {
        allocated_textures.erase(texture);
        m_rhi->DestroyTexture(texture);
    }
}

Container::FixedArray<Vertex, 24> cube_vertices{
    // Top
    Vertex{Math::float3(-1, 1, -1)}, // 0
    Vertex{Math::float3(1, 1, -1)},  // 1
    Vertex{Math::float3(-1, 1, 1)},  // 2
    Vertex{Math::float3(1, 1, 1)},   // 3

    // Bottom
    Vertex{Math::float3(-1, -1, -1)}, // 4
    Vertex{Math::float3(1, -1, -1)},  // 5
    Vertex{Math::float3(-1, -1, 1)},  // 6
    Vertex{Math::float3(1, -1, 1)},   // 7

    // Front
    Vertex{Math::float3(-1, 1, 1)},  // 8
    Vertex{Math::float3(1, 1, 1)},   // 9
    Vertex{Math::float3(-1, -1, 1)}, // 10
    Vertex{Math::float3(1, -1, 1)},  // 11

    // Back
    Vertex{Math::float3(-1, 1, -1)},  // 12
    Vertex{Math::float3(1, 1, -1)},   // 13
    Vertex{Math::float3(-1, -1, -1)}, // 14
    Vertex{Math::float3(1, -1, -1)},  // 15

    // Left
    Vertex{Math::float3(-1, 1, 1)},   // 16
    Vertex{Math::float3(-1, 1, -1)},  // 17
    Vertex{Math::float3(-1, -1, 1)},  // 18
    Vertex{Math::float3(-1, -1, -1)}, // 19

    // Right
    Vertex{Math::float3(1, 1, 1)},  // 20
    Vertex{Math::float3(1, 1, -1)}, // 21
    Vertex{Math::float3(1, -1, 1)}, // 22
    Vertex{Math::float3(1, -1, -1)} // 23

};

Container::FixedArray<Index, 36> cube_indices{// Top
                                   0, 1, 2, 2, 3, 1,
                                   // Bottom
                                   4, 5, 6, 6, 7, 5,
                                   // Front
                                   8, 9, 10, 10, 11, 9,
                                   // Back
                                   12, 13, 14, 14, 15, 13,
                                   // Left
                                   16, 17, 18, 18, 19, 17,
                                   // Right
                                   20, 21, 22, 22, 23, 21};

} // namespace Horizon