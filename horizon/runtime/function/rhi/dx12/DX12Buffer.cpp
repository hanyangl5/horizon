
#include <runtime/function/rhi/dx12/DX12Buffer.h>

namespace Horizon::RHI {

DX12Buffer::DX12Buffer(D3D12MA::Allocator *allocator,
                       const BufferCreateInfo &buffer_create_info,
                       MemoryFlag memory_flag) noexcept
    : Buffer(buffer_create_info), m_allocator(allocator) {
    m_size = buffer_create_info.size;
    m_usage = buffer_create_info.buffer_usage_flags;

    D3D12_RESOURCE_FLAGS usage{
        ToDX12BufferUsage(buffer_create_info.buffer_usage_flags)};
    // Alignment must be 64KB (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) or 0,
    // which is effectively 64KB.
    auto _buffer_create_info = CD3DX12_RESOURCE_DESC::Buffer(
        buffer_create_info.size, usage,
        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

    D3D12MA::ALLOCATION_DESC allocation_desc{};
    D3D12_RESOURCE_STATES initial_state{};
    if (memory_flag == MemoryFlag::CPU_VISABLE_MEMORY) {
        allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        initial_state = D3D12_RESOURCE_STATE_GENERIC_READ;
    } else if (memory_flag == MemoryFlag::DEDICATE_GPU_MEMORY) {
        allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
        initial_state = D3D12_RESOURCE_STATE_COMMON;
    }

    CHECK_DX_RESULT(allocator->CreateResource(
        &allocation_desc, &_buffer_create_info, initial_state, NULL,
        &m_allocation, IID_NULL, NULL));
}

DX12Buffer::~DX12Buffer() noexcept { m_allocation->Release(); }

void *DX12Buffer::GetBufferPointer() noexcept {
    return m_allocation->GetResource();
}
} // namespace Horizon::RHI