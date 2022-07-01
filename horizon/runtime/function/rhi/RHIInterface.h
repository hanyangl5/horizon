#pragma once

#include <memory>
#include <unordered_map>
#include <thread>
#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/CommandList.h>
#include <runtime/function/rhi/CommandContext.h>
#include <runtime/function/shader_compiler/ShaderCompiler.h>
#include <runtime/core/jobsystem/ThreadPool.h>

namespace Horizon
{
	namespace RHI
	{
		class RHIInterface
		{
		public:
			RHIInterface() noexcept;
			~RHIInterface() noexcept = default;

			virtual void InitializeRenderer() noexcept = 0;

			virtual Buffer* CreateBuffer(const BufferCreateInfo& buffer_create_info) noexcept = 0;
			virtual void DestroyBuffer(Buffer* buffer) noexcept = 0;

			virtual Texture* CreateTexture(const TextureCreateInfo& texture_create_info) noexcept = 0;
			virtual void DestroyTexture(Texture* texture) noexcept = 0;

			virtual void CreateSwapChain(std::shared_ptr<Window> window) noexcept = 0;

			virtual ShaderProgram* CreateShaderProgram(
				ShaderTargetStage stage,
				const std::string& entry_point,
				u32 compile_flags,
				std::string file_name) noexcept = 0;
			// virtual void CreateRenderTarget() = 0;
			virtual Pipeline* CreatePipeline(const PipelineCreateInfo& pipeline_create_info) noexcept = 0;
			// virtual void CreateDescriptorSet() = 0;
			virtual CommandList* GetCommandList(CommandQueueType type) noexcept = 0;
			virtual void ResetCommandResources() noexcept = 0;

			// submit command list to command queue
			virtual void SubmitCommandList(CommandList** command_list) noexcept = 0;
		protected:
			u32 m_back_buffer_count = 2;
			u32 m_current_frame_index = 0;
			std::shared_ptr<ShaderCompiler> m_shader_compiler = nullptr;
			// each thread has one command pool, 
			std::unordered_map<std::thread::id, CommandContext*> m_command_context_map;
		public:
			//std::unique_ptr<ThreadPool> m_thread_pool;
		private:
			std::shared_ptr<Window> m_window = nullptr;
		};

	}
}