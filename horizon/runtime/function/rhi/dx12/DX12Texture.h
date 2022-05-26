#pragma once

#include <third_party/D3D12MemoryAllocator/include/D3D12MemAlloc.h>

#include "stdafx.h"
#include <runtime/function/rhi/Texture.h>  

namespace Horizon {
	namespace RHI {

		class DX12Texture : public Texture2
		{
		public:
			DX12Texture(D3D12MA::Allocator* allocator, const TextureCreateInfo& texture_create_info);
			~DX12Texture();
		private:
			virtual void Destroy() override;
		private:
			D3D12MA::Allocation* m_allocation;
			D3D12MA::Allocator* m_allocator;
		};

	}
}