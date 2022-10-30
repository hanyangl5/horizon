#include "ResourceManager.h"

namespace Horizon {
ResourceManager::ResourceManager(Backend::RHI *rhi) noexcept : m_rhi(rhi) {}

ResourceManager::~ResourceManager() noexcept {

    for (auto &remain_buffer : allocated_buffers) {
        if (remain_buffer != nullptr) {
            allocated_buffers.erase(remain_buffer);
            delete remain_buffer;
        }
    }
}

Buffer *ResourceManager::CreateGpuBuffer(const BufferCreateInfo &buffer_create_info, const std::string &name) {
    return nullptr;
}

Buffer *ResourceManager::GetEmptyVertexBuffer() {

    if (empty_vertex_buffer == nullptr) {

        BufferCreateInfo vertex_buffer_create_info{};
        vertex_buffer_create_info.size = 1;
        vertex_buffer_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_VERTEX_BUFFER;
        vertex_buffer_create_info.initial_state = ResourceState::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        empty_vertex_buffer = m_rhi->CreateBuffer1(vertex_buffer_create_info);
    }
    return empty_vertex_buffer;
}

void ResourceManager::DestroyGpuBuffer(Buffer *buffer) {
    if (allocated_buffers.find(buffer) != allocated_buffers.end()) {
        allocated_buffers.erase(buffer);
        delete buffer;
        buffer = nullptr;
    }
}
Texture *ResourceManager::CreateGpuTexture(const TextureCreateInfo &texture_create_info, const std::string &name) {
    return nullptr;
}
void ResourceManager::DestroyGpuTexture(Texture *texture) {

}
} // namespace Horizon