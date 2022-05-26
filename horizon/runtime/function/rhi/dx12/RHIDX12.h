#pragma once

#include <string>
#include <vector>

#include <third_party/D3D12MemoryAllocator/include/D3D12MemAlloc.h>

#include <runtime/core/log/Log.h>
#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/RHIInterface.h>
#include <runtime/function/rhi/RenderContext.h>
#include <runtime/function/rhi/dx12/DX12Buffer.h>
#include <runtime/function/rhi/dx12/DX12Texture.h>

namespace Horizon {
	namespace RHI {
		class RHIDX12 : public RHIInterface
		{
		public:
			RHIDX12();
			virtual ~RHIDX12();

			virtual void InitializeRenderer() override;

			virtual Buffer* CreateBuffer(const BufferCreateInfo& create_info) override;
			virtual void DestroyBuffer(Buffer* buffer) override;

			virtual Texture2* CreateTexture(const TextureCreateInfo& texture_create_info) override;
			virtual void DestroyTexture(Texture2* texture) override;

			virtual void CreateSwapChain(std::shared_ptr<Window> window) override;
		private:
			void InitializeDX12Renderer();
			void CreateFactory();
			void PickGPU(IDXGIFactory6* factory, IDXGIAdapter4** gpu);
			void CreateDevice();
			void InitializeD3DMA();

		private:
			struct DX12RendererContext {
				ID3D12Device* device;
				IDXGIFactory6* factory;
				IDXGIAdapter4* active_gpu;
				D3D12MA::Allocator* d3dma_allocator;
				ID3D12CommandQueue* graphics_queue, * compute_queue, * transfer_queue;
				IDXGISwapChain3* swap_chain;
			}m_dx12;
		};
	}

}