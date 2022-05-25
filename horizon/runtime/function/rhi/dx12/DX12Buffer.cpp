
#include "DX12Buffer.h"

namespace Horizon {
	namespace RHI {

		DX12Buffer::DX12Buffer(D3D12MA::Allocator* allocator, D3D12_RESOURCE_DESC* buffer_create_info) :m_allocator(allocator)
		{
			D3D12MA::ALLOCATION_DESC allocation_desc = {};
			allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

			CHECK_DX_RESULT(allocator->CreateResource(
				&allocation_desc,
				buffer_create_info,
				D3D12_RESOURCE_STATE_COPY_DEST,
				NULL,
				&m_allocation,
				IID_NULL, NULL));
		}

		DX12Buffer::~DX12Buffer()
		{
			Destroy();
		}

		void DX12Buffer::Destroy()
		{
			m_allocation->Release();
		}
	}
}