#include "../RenderSystem.h"
#include <runtime/core/utils/renderdoc/RenderDoc.h>

namespace Horizon {

void RenderSystem::RunRenderTest() {

    m_render_api->ResetCommandResources();

    {
        // BUFFER CREATION TEST

        auto buffer = m_render_api->CreateBuffer(
            BufferCreateInfo{BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER, 32});
        m_render_api->DestroyBuffer(buffer);

        // TEXTURE CREATION TEST

        auto texture = m_render_api->CreateTexture(
            TextureCreateInfo{TextureType::TEXTURE_TYPE_2D,
                              TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                              TextureUsage::TEXTURE_USAGE_R, 4, 4, 1});
        m_render_api->DestroyTexture(texture);
    }

    // data uploading,
    {
        static bool buffer_created = false;
        static RHI::Buffer *buffer = nullptr;
        if (!buffer_created) {
            buffer = m_render_api->CreateBuffer(
                BufferCreateInfo{BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER |
                                     BufferUsage::BUFFER_USAGE_DYNAMIC_UPDATE,
                                 sizeof(Math::vec3)});
            buffer_created = true;
        }

        // dynamic buffer, cpu pointer not change, cpu data change, gpu data
        // change
        {
            static Math::vec3 data(1.0);

            auto transfer =
                m_render_api->GetCommandList(CommandQueueType::TRANSFER);

            // data update per frame
            data += Math::vec3(1.0);

            transfer->BeginRecording();

            transfer->UpdateBuffer(buffer, &data, sizeof(data));

            transfer->EndRecording();
        }

        static bool buffer_created2 = false;
        static RHI::Buffer *buffer2 = nullptr;
        if (!buffer_created2) {
            buffer2 = m_render_api->CreateBuffer(
                BufferCreateInfo{BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER |
                                     BufferUsage::BUFFER_USAGE_DYNAMIC_UPDATE,
                                 sizeof(Math::vec3)});
            buffer_created2 = true;
        }

        // dynamic buffer, cpu pointer change, cpu data change, gpu data change
        {
            Math::vec3 data2(1.0);
            auto transfer =
                m_render_api->GetCommandList(CommandQueueType::TRANSFER);

            // data update per frame
            data2 += rand();

            transfer->BeginRecording();

            transfer->UpdateBuffer(buffer2, &data2, sizeof(data2));

            transfer->EndRecording();
        }

        static bool buffer_created3 = false;
        static RHI::Buffer *buffer3 = nullptr;
        if (!buffer_created3) {
            buffer3 = m_render_api->CreateBuffer(BufferCreateInfo{
                BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER, sizeof(Math::vec3)});
            buffer_created3 = true;
        }

        // static buffer, cpu pointer not change, cpu data not change, gpu data
        // not change
        {
            Math::vec3 data3(1.0);
            auto transfer =
                m_render_api->GetCommandList(CommandQueueType::TRANSFER);

            // data update per frame
            data3 += rand();

            transfer->BeginRecording();

            // can only update once
            // transfer->UpdateBuffer(buffer3, &data3, sizeof(data3));

            transfer->EndRecording();
        }
        // m_render_api->DestroyBuffer(buffer);
    }

    // multithread command list recording
    {

        // m_render_api->m_thread_pool->enqueue([&]() {
        auto graphics =
            m_render_api->GetCommandList(CommandQueueType::GRAPHICS);
        // graphics->Dispatch(u32 group_count_x, u32 group_count_y, u32
        // group_count_z);
        graphics->BeginRecording();
        // graphics->Dispatch(u32 group_count_x, u32 group_count_y, u32
        // group_count_z);
        graphics->Draw();
        graphics->EndRecording();

        auto compute = m_render_api->GetCommandList(CommandQueueType::COMPUTE);
        compute->BeginRecording();
        // compute->Dispatch(1, 1, 1);
        compute->EndRecording();

        auto transfer =
            m_render_api->GetCommandList(CommandQueueType::TRANSFER);

        transfer->BeginRecording();

        transfer->EndRecording();
        //});
    }

    int a = 0;

    // shader compiation
    {
        std::string file_name =
            "D:/codes/horizon/horizon/assets/shaders/hlsl/shader.hlsl";
        auto shader_program = m_render_api->CreateShaderProgram(
            ShaderType::VERTEX_SHADER, "vs_main", 0, file_name);

        m_render_api->DestroyShaderProgram(shader_program);

        // destroy
    }

    // pipeline

    {
        static bool create_ed = false;
        static ShaderProgram *shader = nullptr;
        static Pipeline *pipeline = nullptr;
        if (!create_ed) {
            create_ed = true;
            std::string file_name =
                "D:/codes/horizon/horizon/assets/shaders/hlsl/cs.hlsl";
            shader = m_render_api->CreateShaderProgram(
                ShaderType::COMPUTE_SHADER, "cs_main", 0,
                file_name); // TODO: shader resource release
            pipeline = m_render_api->CreatePipeline(
                PipelineCreateInfo{PipelineType::COMPUTE});
        }
        auto cl = m_render_api->GetCommandList(CommandQueueType::COMPUTE);
        pipeline->SetShader(shader);
        cl->BeginRecording();
        cl->BindPipeline(pipeline);
        cl->Dispatch(1, 1, 1);
        cl->EndRecording();

        m_render_api->SubmitCommandLists(COMPUTE, std::vector{cl});
    }
}

} // namespace Horizon