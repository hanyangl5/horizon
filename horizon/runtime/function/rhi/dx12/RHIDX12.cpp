#include "RHIDX12.h"
#include "DX12Config.h"

namespace Horizon {
	namespace RHI {

		RHIDX12::RHIDX12()
		{
		}

		RHIDX12::~RHIDX12()
		{
			m_dx12.d3dma_allocator->Release();
			m_dx12.active_gpu->Release();
			m_dx12.device->Release();
			m_dx12.factory->Release();

		}

		void RHIDX12::InitializeRenderer()
		{
			LOG_INFO("using DirectX 12 renderer");
			InitializeDX12Renderer();
		}

		Buffer* RHIDX12::CreateBuffer(BufferCreateInfo create_info)
		{
			auto buffer_create_info = CD3DX12_RESOURCE_DESC::Buffer(create_info.size);
			//m_dx12.device->CreateCommittedResource();
			//D3D12_RESOURCE_DESC _create_info{};
			//_create_info.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			//_create_info.Alignment = 0;
			//_create_info.Width = create_info.size;
			//_create_info.Height = 1;
			//_create_info.DepthOrArraySize = 1;
			//_create_info.MipLevels = 1;
			//_create_info.Format = DXGI_FORMAT_UNKNOWN;
			//_create_info.SampleDesc.Count = 1;
			//_create_info.SampleDesc.Quality = 0;
			//_create_info.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			//_create_info.Flags = D3D12_RESOURCE_FLAG_NONE;
			//_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			//_create_info.size = create_info.size;
			//_create_info.usage = ToVulkanBufferUsage(create_info.buffer_usage_flags);
			//Buffer* buffer = new DX12Buffer(m_dx12.d3dma_allocator, &_create_info);
			//return buffer;
		}

		void RHIDX12::DestroyBuffer(Buffer* buffer)
		{
			delete buffer;
		}

		void RHIDX12::CreateTexture()
		{
			//auto texture_create_info = CD3DX12_RESOURCE_DESC;
		}

		void RHIDX12::CreateSwapChain(std::shared_ptr<Window> window)
		{
			DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
			swap_chain_desc.BufferCount = m_back_buffer_count;
			swap_chain_desc.Width = window->GetWidth();
			swap_chain_desc.Height = window->GetHeight();
			swap_chain_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swap_chain_desc.SampleDesc.Count = 1;
			swap_chain_desc.SampleDesc.Quality = 0;
			swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

			IDXGISwapChain1* swap_chain;

			CHECK_DX_RESULT(m_dx12.factory->CreateSwapChainForHwnd(
				m_dx12.graphics_queue,        // Swap chain needs the queue so that it can force a flush on it.
				window->GetWin32Window(),
				&swap_chain_desc,
				nullptr,
				nullptr,
				&swap_chain
			));

			m_dx12.swap_chain = static_cast<IDXGISwapChain3*>(swap_chain);
			m_current_frame_index = m_dx12.swap_chain->GetCurrentBackBufferIndex();
		}

		void RHIDX12::InitializeDX12Renderer() {

			CreateFactory();
			CreateDevice();
			InitializeD3DMA();
		}

		void RHIDX12::CreateFactory() {
			u32 dxgi_factory_flags = 0;
#ifndef NDEBUG
			dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
			CHECK_DX_RESULT(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_dx12.factory)));
		}

		void RHIDX12::PickGPU(IDXGIFactory6* factory, IDXGIAdapter4** gpu)
		{
			u32 gpu_count = 0;
			IDXGIAdapter4* adapter = NULL;
			IDXGIFactory6* factory6;

			CHECK_DX_RESULT(factory->QueryInterface(IID_PPV_ARGS(&factory6)))

				// Find number of usable GPUs
				// Use DXGI6 interface which lets us specify gpu preference so we dont need to use NVOptimus or AMDPowerExpress exports
				for (u32 i = 0; DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(
					i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)); ++i)
				{
					DXGI_ADAPTER_DESC3 desc;
					adapter->GetDesc3(&desc);
					if (SUCCEEDED(D3D12CreateDevice(adapter, DX12_API_VERSION, _uuidof(ID3D12Device), nullptr))) {
						break;
					}
				}
			*gpu = adapter;

		}

		void RHIDX12::CreateDevice() {

			// pick gpu
			PickGPU(m_dx12.factory, &m_dx12.active_gpu);

			CHECK_DX_RESULT(D3D12CreateDevice(
				m_dx12.active_gpu,
				DX12_API_VERSION,
				IID_PPV_ARGS(&m_dx12.device)
			));

			// create command queue

			D3D12_COMMAND_QUEUE_DESC graphics_command_queue_desc = {};
			graphics_command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			graphics_command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			CHECK_DX_RESULT(m_dx12.device->CreateCommandQueue(&graphics_command_queue_desc, IID_PPV_ARGS(&m_dx12.graphics_queue)));

#ifdef USE_ASYNC_COMPUTE
			D3D12_COMMAND_QUEUE_DESC compute_command_queue_desc = {};
			compute_command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			compute_command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

			CHECK_DX_RESULT(m_dx12.device->CreateCommandQueue(&compute_command_queue_desc, IID_PPV_ARGS(&m_dx12.compute_queue)));

			D3D12_COMMAND_QUEUE_DESC transfer_command_queue_desc = {};
			transfer_command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			transfer_command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

			CHECK_DX_RESULT(m_dx12.device->CreateCommandQueue(&transfer_command_queue_desc, IID_PPV_ARGS(&m_dx12.transfer_queue)));

#endif // USE_ASYNC_COMPUTE

		}

		void RHIDX12::InitializeD3DMA()
		{
			D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
			allocatorDesc.pDevice = m_dx12.device;
			allocatorDesc.pAdapter = m_dx12.active_gpu;

			CHECK_DX_RESULT(D3D12MA::CreateAllocator(&allocatorDesc, &m_dx12.d3dma_allocator));
		}


	}
}