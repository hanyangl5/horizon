#pragma once

#include <memory>

#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/rhi/RenderContext.h>

namespace Horizon {
	namespace RHI {
		class RHIInterface
		{
		public:
			RHIInterface() = default;
			~RHIInterface() = default;

			virtual void InitializeRenderer() = 0;

			virtual Buffer* CreateBuffer(const BufferCreateInfo& buffer_create_info) = 0;
			virtual void DestroyBuffer(Buffer* buffer) = 0;

			virtual Texture2* CreateTexture(const TextureCreateInfo& texture_create_info) = 0;
			virtual void DestroyTexture(Texture2* texture) = 0;

			virtual void CreateSwapChain(std::shared_ptr<Window> window) = 0;
			//virtual void CreateRenderTarget() = 0;
			//virtual void CreatePipeline() = 0;
			//virtual void CreateDescriptorSet() = 0;
		protected:
			u32 m_back_buffer_count = 2;
			u32 m_current_frame_index = 0;
		private:
			std::shared_ptr<Window> m_window = nullptr;
		};




	}
}