#include "../RenderSystem.h"

namespace Horizon {

	void RenderSystem::RunRenderTest() {


		m_render_api->ResetCommandResources();

		{
			// BUFFER CREATION TEST

			auto buffer = m_render_api->CreateBuffer(BufferCreateInfo{ BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER, 32 });
			m_render_api->DestroyBuffer(buffer);

			// TEXTURE CREATION TEST

			auto texture = m_render_api->CreateTexture(TextureCreateInfo{ TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, TextureUsage::TEXTURE_USAGE_R, 4, 4, 1 });
			m_render_api->DestroyTexture(texture);
		}

		//// shader complition
		//{
		//	std::string file_name = "C:/Users/hylu/OneDrive/mycode/horizon/assets/shaders/hlsl/shader.hlsl";
		//	m_render_api->CreateShaderProgram(ShaderTargetStage::vs, "vs_main", 0, file_name);
		//}
		//// command list recording
		{

			auto graphics = m_render_api->GetCommandList(RHI::CommandQueueType::GRAPHICS);
			//graphics->Dispatch();
			graphics->BeginRecording();
			//graphics->Dispatch();
			graphics->Draw();
			graphics->EndRecording();

			auto compute = m_render_api->GetCommandList(RHI::CommandQueueType::COMPUTE);
			compute->BeginRecording();
			compute->Dispatch();
			compute->EndRecording();

		}

		//// data uploading
		//{
		//	auto transfer = m_render_api->GetCommandList(RHI::CommandQueueType::TRANSFER);

		//	transfer->BeginRecording();
		//	//auto buffer = m_render_api->CreateBuffer(BufferCreateInfo{ BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER, sizeof(Math::vec3) });
		//	//Math::vec3 data(1.0);
		//	//transfer->UpdateBuffer(buffer, &data, sizeof(data));

		//	//// barrier for queue family ownership transfer

		//	//BufferMemoryBarrierDesc bmb{
		//	//	buffer->GetBufferPointer(),
		//	//	0,
		//	//	buffer->GetBufferSize(),
		//	//	MemoryAccessFlags::ACCESS_TRANSFER_READ_BIT,
		//	//	0,
		//	//	RHI::CommandQueueType::TRANSFER,
		//	//	RHI::CommandQueueType::COMPUTE,
		//	//};

		//	//BarrierDesc desc{};
		//	//desc.src_stage = PipelineStageFlags::PIPELINE_STAGE_TRANSFER_BIT;
		//	//desc.dst_stage = PipelineStageFlags::PIPELINE_STAGE_ALL_COMMANDS_BIT;
		//	//desc.buffer_memory_barriers.emplace_back(bmb);
		//	//transfer->InsertBarrier(desc);

		//	transfer->EndRecording();
		//	//m_render_api->DestroyBuffer(buffer);
		//}
	}
}