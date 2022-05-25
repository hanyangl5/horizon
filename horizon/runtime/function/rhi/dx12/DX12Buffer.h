#pragma once

#include <third_party/D3D12MemoryAllocator/include/D3D12MemAlloc.h>

#include "stdafx.h"
#include <runtime/function/rhi/Buffer.h>  

namespace Horizon {
	namespace RHI {

		class DX12Buffer : public Buffer
		{
		public:
			DX12Buffer(D3D12MA::Allocator* allocator, D3D12_RESOURCE_DESC* buffer_create_info);
			~DX12Buffer();
		private:
			virtual void Destroy() override;
		private:
			D3D12MA::Allocation* m_allocation;
			D3D12MA::Allocator* m_allocator;
		};

	}
}