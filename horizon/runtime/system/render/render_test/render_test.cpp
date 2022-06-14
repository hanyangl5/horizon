#include "../RenderSystem.h"

namespace Horizon {

	void RenderSystem::RunRenderTest() {

		{
			// BUFFER TEST

			auto buffer = m_render_api->CreateBuffer(BufferCreateInfo{ BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER, 32 });
			m_render_api->DestroyBuffer(buffer);

			// TEXTURE TEST

			auto texture = m_render_api->CreateTexture(TextureCreateInfo{ TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, TextureUsage::TEXTURE_USAGE_R, 4, 4, 1 });
			m_render_api->DestroyTexture(texture);
		}

		{
			std::string file_name = "C:/Users/hylu/OneDrive/mycode/horizon/assets/shaders/hlsl/shader.hlsl";
			m_render_api->CreateShaderProgram(ShaderTargetStage::vs, "vs_main", 0, file_name);
		}

		{

			auto graphics = m_render_api->GetCommandList(RHI::CommandQueueType::GRAPHICS);
			graphics->Dispatch();
			graphics->BeginRecording();
			graphics->Dispatch();
			graphics->Draw();
			graphics->EndRecording();

			auto compute = m_render_api->GetCommandList(RHI::CommandQueueType::COMPUTE);
			compute->BeginRecording();
			compute->Dispatch();
			compute->EndRecording();

			auto transfer = m_render_api->GetCommandList(RHI::CommandQueueType::TRANSFER);

			transfer->BeginRecording();
			auto buffer = m_render_api->CreateBuffer(BufferCreateInfo{ BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER, sizeof(Math::vec3) });
			Math::vec3 data(1.0);
			transfer->UpdateBuffer(buffer, &data, sizeof(data));

			// barrier for queue family ownership transfer

			BufferMemoryBarrierDesc bmb{
				buffer->GetBufferPointer(),
				0,
				buffer->GetBufferSize(),
				MemoryAccessFlags::ACCESS_TRANSFER_READ_BIT,
				0,
				RHI::CommandQueueType::TRANSFER,
				RHI::CommandQueueType::COMPUTE,
			};

			BarrierDesc desc{};
			desc.src_stage = PipelineStageFlags::PIPELINE_STAGE_TRANSFER_BIT;
			desc.dst_stage = PipelineStageFlags::PIPELINE_STAGE_ALL_COMMANDS_BIT;
			desc.buffer_memory_barriers.emplace_back(bmb);
			transfer->InsertBarrier(desc);

			transfer->EndRecording();

			m_render_api->ResetCommandResources();

			auto compute2 = m_render_api->GetCommandList(RHI::CommandQueueType::COMPUTE);
			compute2->BeginRecording();
			compute2->Dispatch();
			compute2->EndRecording();

			auto compute3 = m_render_api->GetCommandList(RHI::CommandQueueType::COMPUTE);
			compute3->BeginRecording();
			compute3->Dispatch();
			compute3->EndRecording();
		}
	}
}