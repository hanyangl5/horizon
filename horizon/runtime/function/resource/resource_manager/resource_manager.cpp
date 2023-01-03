/*****************************************************************//**
 * \file   resource_manager.cpp
 * \brief  
 * 
 * \author hylu
 * \date   January 2023
 *********************************************************************/

#include "resource_manager.h"

// standard libraries

// third party libraries

// project headers

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

Container::FixedArray<Vertex, 36> cube_vertices{
    // Top
    Vertex{Math::float3(1, -1, 1), Math::float3(-0, -1, 0), Math::float2(0, 0)},
    Vertex{Math::float3(-1, -1, -1), Math::float3(-0, -1, 0), Math::float2(-1, 1)},
    Vertex{Math::float3(1, -1, -1), Math::float3(-0, -1, 0), Math::float2(0, 1)},
    Vertex{Math::float3(-1, 1, -1), Math::float3(0, 1, -0), Math::float2(0, 0)},
    Vertex{Math::float3(0.999999, 1, 1.000001), Math::float3(0, 1, -0), Math::float2(1, -1)},
    Vertex{Math::float3(1, 1, -0.999999), Math::float3(0, 1, -0), Math::float2(1, 0)},
    Vertex{Math::float3(1, 1, -0.999999), Math::float3(1, -0, -0), Math::float2(1, 0)},
    Vertex{Math::float3(1, -1, 1), Math::float3(1, -0, -0), Math::float2(0, -1)},
    Vertex{Math::float3(1, -1, -1), Math::float3(1, -0, -0), Math::float2(1, -1)},
    Vertex{Math::float3(0.999999, 1, 1.000001), Math::float3(-0, -0, 1), Math::float2(1, 0)},
    Vertex{Math::float3(-1, -1, 1), Math::float3(-0, -0, 1), Math::float2(-0, -1)},
    Vertex{Math::float3(1, -1, 1), Math::float3(-0, -0, 1), Math::float2(1, -1)},
    Vertex{Math::float3(-1, -1, 1), Math::float3(-1, -0, -0), Math::float2(0, 0)},
    Vertex{Math::float3(-1, 1, -1), Math::float3(-1, -0, -0), Math::float2(1, 1)},
    Vertex{Math::float3(-1, -1, -1), Math::float3(-1, -0, -0), Math::float2(1, 0)},
    Vertex{Math::float3(1, -1, -1), Math::float3(0, 0, -1), Math::float2(0, 0)},
    Vertex{Math::float3(-1, 1, -1), Math::float3(0, 0, -1), Math::float2(-1, 1)},
    Vertex{Math::float3(1, 1, -0.999999), Math::float3(0, 0, -1), Math::float2(0, 1)},
    Vertex{Math::float3(1, -1, 1), Math::float3(0, -1, 0), Math::float2(0, 0)},
    Vertex{Math::float3(-1, -1, 1), Math::float3(0, -1, 0), Math::float2(-1, 0)},
    Vertex{Math::float3(-1, -1, -1), Math::float3(0, -1, 0), Math::float2(-1, 1)},
    Vertex{Math::float3(-1, 1, -1), Math::float3(0, 1, 0), Math::float2(0, 0)},
    Vertex{Math::float3(-1, 1, 1), Math::float3(0, 1, 0), Math::float2(-0, -1)},
    Vertex{Math::float3(0.999999, 1, 1.000001), Math::float3(0, 1, 0), Math::float2(1, -1)},
    Vertex{Math::float3(1, 1, -0.999999), Math::float3(1, 0, 1e-06), Math::float2(1, 0)},
    Vertex{Math::float3(0.999999, 1, 1.000001), Math::float3(1, 0, 1e-06), Math::float2(-0, 0)},
    Vertex{Math::float3(1, -1, 1), Math::float3(1, 0, 1e-06), Math::float2(0, -1)},
    Vertex{Math::float3(0.999999, 1, 1.000001), Math::float3(-0, 0, 1), Math::float2(1, 0)},
    Vertex{Math::float3(-1, 1, 1), Math::float3(-0, 0, 1), Math::float2(-0, 0)},
    Vertex{Math::float3(-1, -1, 1), Math::float3(-0, 0, 1), Math::float2(-0, -1)},
    Vertex{Math::float3(-1, -1, 1), Math::float3(-1, -0, -0), Math::float2(0, 0)},
    Vertex{Math::float3(-1, 1, 1), Math::float3(-1, -0, -0), Math::float2(0, 1)},
    Vertex{Math::float3(-1, 1, -1), Math::float3(-1, -0, -0), Math::float2(1, 1)},
    Vertex{Math::float3(1, -1, -1), Math::float3(0, 0, -1), Math::float2(0, 0)},
    Vertex{Math::float3(-1, -1, -1), Math::float3(0, 0, -1), Math::float2(-1, 0)},
    Vertex{Math::float3(-1, 1, -1), Math::float3(0, 0, -1), Math::float2(-1, 1)}

};

Container::FixedArray<Index, 36> cube_indices{0,  1,  2,  3,  4,  5,  6,  7,  8,   9,  10, 11, 12, 13, 14, 15, 16, 17,
                                              18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35};
;

} // namespace Horizon
