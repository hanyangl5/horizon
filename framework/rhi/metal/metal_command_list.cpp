#include "metal_command_list.h"

#include <core/log.h>
#include <rhi/metal/metal_buffer.h>
#include <rhi/metal/metal_pipeline.h>
#include <rhi/metal/metal_render_target.h>
#include <rhi/metal/metal_swap_chain.h>
#include <rhi/metal/metal_texture.h>

#include <cstring>

namespace Horizon::Backend
{

MetalCommandList::MetalCommandList(MTL::CommandQueue *command_queue) noexcept
    : CommandList(CommandQueueType::GRAPHICS), m_command_queue(command_queue)
{
}

MetalCommandList::~MetalCommandList() noexcept
{
    if (m_render_encoder != nil)
    {
        m_render_encoder->endEncoding();
        m_render_encoder = nil;
    }
    if (m_command_buffer != nil)
    {
        m_command_buffer->release();
        m_command_buffer = nil;
    }
}

void MetalCommandList::BeginRecording()
{
    if (m_render_encoder != nil)
    {
        m_render_encoder->endEncoding();
        m_render_encoder = nil;
    }
    if (m_command_buffer != nil)
    {
        m_command_buffer->release();
        m_command_buffer = nil;
    }

    m_command_buffer = static_cast<MTL::CommandBuffer *>(m_command_queue->commandBuffer()->retain());
    m_bound_pipeline = nullptr;
    m_index_buffer = nullptr;
    m_index_buffer_offset = 0;
    m_present_swap_chain = nullptr;
}

void MetalCommandList::EndRecording()
{
    if (m_render_encoder != nil)
    {
        m_render_encoder->endEncoding();
        m_render_encoder = nil;
    }
}

void MetalCommandList::BindVertexBuffers(u32 buffer_count, Buffer **buffers, u32 *offsets)
{
    if (m_render_encoder == nil)
    {
        LOG_ERROR("Metal BindVertexBuffers called outside render pass");
        return;
    }

    for (u32 index = 0; index < buffer_count; ++index)
    {
        auto *buffer = static_cast<MetalBuffer *>(buffers[index]);
        const u32 offset = offsets != nullptr ? offsets[index] : 0;
        m_render_encoder->setVertexBuffer(buffer->m_buffer, offset, index);
    }
}

void MetalCommandList::BindIndexBuffer(Buffer *buffer, u32 offset)
{
    m_index_buffer = static_cast<MetalBuffer *>(buffer);
    m_index_buffer_offset = offset;
}

void MetalCommandList::BeginRenderPass(const RenderPassBeginInfo &begin_info)
{
    if (m_command_buffer == nil)
    {
        LOG_ERROR("Metal BeginRenderPass called without command buffer");
        return;
    }

    auto *render_target = static_cast<MetalRenderTarget *>(begin_info.render_targets[0].data);
    auto *texture = static_cast<MetalTexture *>(render_target->GetTexture());
    if (texture == nullptr || texture->m_texture == nil)
    {
        LOG_ERROR("Metal render pass target texture is invalid");
        return;
    }

    MTL::RenderPassDescriptor *descriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
    auto *color_attachment = descriptor->colorAttachments()->object(0);
    color_attachment->setTexture(texture->m_texture);
    color_attachment->setLoadAction(
        begin_info.render_targets[0].load_op == RenderTargetLoadOp::CLEAR ? MTL::LoadActionClear : MTL::LoadActionLoad);
    color_attachment->setStoreAction(MTL::StoreActionStore);

    if (const auto *clear_color = std::get_if<ClearColorValue>(&begin_info.render_targets[0].clear_color))
    {
        color_attachment->setClearColor(MTL::ClearColor::Make(clear_color->float32[0], clear_color->float32[1],
                                                              clear_color->float32[2], clear_color->float32[3]));
    }

    m_render_encoder = m_command_buffer->renderCommandEncoder(descriptor);
    if (m_render_encoder == nil)
    {
        LOG_ERROR("Failed to create Metal render command encoder");
        return;
    }

    m_render_encoder->setViewport(MTL::Viewport{
        static_cast<double>(begin_info.render_area.x), static_cast<double>(begin_info.render_area.y),
        static_cast<double>(begin_info.render_area.w), static_cast<double>(begin_info.render_area.h), 0.0, 1.0});

    if (texture->m_is_swap_chain_texture)
    {
        m_present_swap_chain = m_active_swap_chain;
    }
}

void MetalCommandList::EndRenderPass()
{
    if (m_render_encoder != nil)
    {
        m_render_encoder->endEncoding();
        m_render_encoder = nil;
    }
}

void MetalCommandList::BeginComputePass(const char *debug_name)
{
    (void)debug_name;
    LOG_ERROR("Metal compute path is not implemented in Phase 1");
}

void MetalCommandList::EndComputePass()
{
}

void MetalCommandList::DrawInstanced(u32 vertex_count, u32 first_vertex, u32 instance_count, u32 first_instance)
{
    if (m_render_encoder == nil)
    {
        LOG_ERROR("Metal DrawInstanced called outside render pass");
        return;
    }
    m_render_encoder->drawPrimitives(m_bound_pipeline != nullptr ? m_bound_pipeline->m_primitive_type
                                                                 : MTL::PrimitiveTypeTriangle,
                                     first_vertex, vertex_count, instance_count, first_instance);
}

void MetalCommandList::DrawIndexedInstanced(u32 index_count, u32 first_index, u32 first_vertex, u32 instance_count,
                                            u32 first_instance)
{
    if (m_render_encoder == nil || m_index_buffer == nullptr)
    {
        LOG_ERROR("Metal DrawIndexedInstanced called without index buffer or render pass");
        return;
    }
    m_render_encoder->drawIndexedPrimitives(
        m_bound_pipeline != nullptr ? m_bound_pipeline->m_primitive_type : MTL::PrimitiveTypeTriangle, index_count,
        MTL::IndexTypeUInt32, m_index_buffer->m_buffer, m_index_buffer_offset + first_index * sizeof(u32),
        instance_count, first_vertex, first_instance);
}

void MetalCommandList::DrawIndirect()
{
    LOG_ERROR("Metal DrawIndirect is not implemented in Phase 1");
}

void MetalCommandList::DrawIndirectIndexedInstanced(Buffer *buffer, u32 offset, u32 draw_count, u32 stride)
{
    (void)buffer;
    (void)offset;
    (void)draw_count;
    (void)stride;
    LOG_ERROR("Metal DrawIndirectIndexedInstanced is not implemented in Phase 1");
}

void MetalCommandList::DrawMeshTasks(u32 group_count_x, u32 group_count_y, u32 group_count_z)
{
    (void)group_count_x;
    (void)group_count_y;
    (void)group_count_z;
    LOG_ERROR("Metal mesh shader path is not implemented in Phase 1");
}

void MetalCommandList::Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z)
{
    (void)group_count_x;
    (void)group_count_y;
    (void)group_count_z;
    LOG_ERROR("Metal compute path is not implemented in Phase 1");
}

void MetalCommandList::DispatchIndirect()
{
    LOG_ERROR("Metal DispatchIndirect is not implemented in Phase 1");
}

void MetalCommandList::UpdateBuffer(Buffer *buffer, void *data, u64 size)
{
    if (buffer == nullptr || data == nullptr)
    {
        LOG_ERROR("Metal UpdateBuffer received null input");
        return;
    }

    auto *metal_buffer = static_cast<MetalBuffer *>(buffer);
    if (metal_buffer->m_buffer == nil)
    {
        LOG_ERROR("Metal buffer is invalid");
        return;
    }

    if (size > buffer->m_size)
    {
        LOG_ERROR("Metal UpdateBuffer size {} exceeds buffer size {}", size, buffer->m_size);
        return;
    }

    std::memcpy(metal_buffer->m_buffer->contents(), data, static_cast<size_t>(size));
}

void MetalCommandList::UpdateTexture(Texture *texture, const TextureUpdateDesc &texture_data)
{
    (void)texture;
    (void)texture_data;
    LOG_ERROR("Metal UpdateTexture is not implemented in Phase 1");
}

void MetalCommandList::CopyBuffer(Buffer *dst_buffer, Buffer *src_buffer)
{
    (void)dst_buffer;
    (void)src_buffer;
    LOG_ERROR("Metal CopyBuffer is not implemented in Phase 1");
}

void MetalCommandList::CopyTexture(Texture *src_texture, Texture *dst_texture)
{
    (void)src_texture;
    (void)dst_texture;
    LOG_ERROR("Metal CopyTexture is not implemented in Phase 1");
}

void MetalCommandList::InsertBarrier(const BarrierDesc &desc)
{
    for (const auto &barrier : desc.buffer_memory_barriers)
    {
        if (barrier.buffer != nullptr)
        {
            barrier.buffer->m_resource_state = barrier.dst_state;
        }
    }
    for (const auto &barrier : desc.texture_memory_barriers)
    {
        if (barrier.texture != nullptr)
        {
            barrier.texture->m_state = barrier.dst_state;
        }
    }
}

void MetalCommandList::BindPipeline(Pipeline *pipeline)
{
    m_bound_pipeline = static_cast<MetalPipeline *>(pipeline);
    if (m_render_encoder == nil || m_bound_pipeline == nullptr)
    {
        return;
    }

    m_render_encoder->setRenderPipelineState(m_bound_pipeline->m_render_pipeline_state);
    if (m_bound_pipeline->m_depth_stencil_state != nil)
    {
        m_render_encoder->setDepthStencilState(m_bound_pipeline->m_depth_stencil_state);
    }
    m_render_encoder->setFrontFacingWinding(m_bound_pipeline->m_front_winding);
    m_render_encoder->setCullMode(m_bound_pipeline->m_cull_mode);
    m_render_encoder->setTriangleFillMode(m_bound_pipeline->m_fill_mode);
    m_render_encoder->setViewport(m_bound_pipeline->m_viewport);
}

void MetalCommandList::BindPushConstant(Pipeline *pipeline, const std::string &name, void *data)
{
    (void)pipeline;
    (void)name;
    (void)data;
}

void MetalCommandList::ClearBuffer(Buffer *buffer, f32 clear_value)
{
    (void)buffer;
    (void)clear_value;
    LOG_ERROR("Metal ClearBuffer is not implemented in Phase 1");
}

void MetalCommandList::ClearTextrue(Texture *texture, const ClearColorValue &clear_value)
{
    (void)texture;
    (void)clear_value;
    LOG_ERROR("Metal ClearTextrue is not implemented in Phase 1");
}

void MetalCommandList::GenerateMipMap(Texture *texture)
{
    (void)texture;
    LOG_ERROR("Metal GenerateMipMap is not implemented in Phase 1");
}

void MetalCommandList::BeginQuery()
{
}

void MetalCommandList::EndQuery()
{
}

void MetalCommandList::SetActiveSwapChain(MetalSwapChain *swap_chain) noexcept
{
    m_active_swap_chain = swap_chain;
}

} // namespace Horizon::Backend
