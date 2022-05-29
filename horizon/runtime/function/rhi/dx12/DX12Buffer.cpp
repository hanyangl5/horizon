
#include "DX12Buffer.h"

namespace Horizon
{
	namespace RHI
	{

		DX12Buffer::DX12Buffer(D3D12MA::Allocator *allocator, const BufferCreateInfo &buffer_create_info) noexcept : Buffer(buffer_create_info), m_allocator(allocator)
		{
			m_size = buffer_create_info.size;
			m_usage = buffer_create_info.buffer_usage_flags;

			D3D12_RESOURCE_FLAGS usage = ToDX12BufferUsage(buffer_create_info.buffer_usage_flags);
			// Alignment must be 64KB (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) or 0, which is effectively 64KB.
			auto _buffer_create_info = CD3DX12_RESOURCE_DESC::Buffer(buffer_create_info.size, usage, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

			D3D12MA::ALLOCATION_DESC allocation_desc = {};
			allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

			CHECK_DX_RESULT(allocator->CreateResource(
				&allocation_desc,
				&_buffer_create_info,
				D3D12_RESOURCE_STATE_COPY_DEST,
				NULL,
				&m_allocation,
				IID_NULL, NULL));
		}

		DX12Buffer::~DX12Buffer() noexcept
		{
			Destroy();
		}

		void DX12Buffer::Destroy() noexcept
		{
			m_allocation->Release();
		}
	}
}