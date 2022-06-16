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

namespace Horizon
{
	namespace RHI
	{
		class RHIDX12 : public RHIInterface
		{
		public:
			RHIDX12() noexcept;
			virtual ~RHIDX12() noexcept;

			virtual void InitializeRenderer() noexcept override;

			virtual Buffer *CreateBuffer(const BufferCreateInfo &create_info) noexcept override;
			virtual void DestroyBuffer(Buffer *buffer) noexcept override;

			virtual Texture *CreateTexture(const TextureCreateInfo &texture_create_info) noexcept override;
			virtual void DestroyTexture(Texture *texture) noexcept override;

			virtual void CreateSwapChain(std::shared_ptr<Window> window) noexcept override;

			virtual ShaderProgram CreateShaderProgram(ShaderTargetStage stage, const std::string& entry_point, u32 compile_flags, std::string file_name) noexcept override;

			virtual CommandList* GetCommandList(CommandQueueType type) noexcept override;

			virtual void ResetCommandResources() noexcept override;

		private:
			void InitializeDX12Renderer() noexcept;
			void CreateFactory() noexcept;
			void PickGPU(IDXGIFactory6 *factory, IDXGIAdapter4 **gpu) noexcept;
			void CreateDevice() noexcept;
			void InitializeD3DMA() noexcept;

		private:
			struct DX12RendererContext
			{
				ID3D12Device *device;
				IDXGIFactory6 *factory;
				IDXGIAdapter4 *active_gpu;
				D3D12MA::Allocator *d3dma_allocator;
				ID3D12CommandQueue *graphics_queue, *compute_queue, *transfer_queue;
				IDXGISwapChain3 *swap_chain;
			} m_dx12;
		};
	}

}