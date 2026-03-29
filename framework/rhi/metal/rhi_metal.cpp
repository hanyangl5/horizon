#include "rhi_metal.h"

#include <core/log.h>
#include <core/memory.h>
#include <rhi/metal/metal_buffer.h>
#include <rhi/metal/metal_command_list.h>
#include <rhi/metal/metal_pipeline.h>
#include <rhi/metal/metal_render_target.h>
#include <rhi/metal/metal_sampler.h>
#include <rhi/metal/metal_semaphore.h>
#include <rhi/metal/metal_shader.h>
#include <rhi/metal/metal_swap_chain.h>
#include <rhi/metal/metal_texture.h>
#include <rhi/metal/metal_utils.h>

#include <algorithm>
#include <cmath>

namespace Horizon::Backend
{

using namespace MetalUtils;

MetalRHI::MetalRHI(bool offscreen) noexcept
{
    m_offscreen = offscreen;
}

MetalRHI::~MetalRHI() noexcept
{
    if (m_last_committed_command_buffer != nil)
    {
        m_last_committed_command_buffer->release();
        m_last_committed_command_buffer = nil;
    }
    if (m_graphics_command_list != nullptr)
    {
        Memory::Free(m_graphics_command_list);
        m_graphics_command_list = nullptr;
    }
    if (m_command_queue != nil)
    {
        m_command_queue->release();
        m_command_queue = nil;
    }
    if (m_device != nil)
    {
        m_device->release();
        m_device = nil;
    }
}

void MetalRHI::InitializeRenderer()
{
    m_device = MTL::CreateSystemDefaultDevice();
    if (m_device == nil)
    {
        LOG_ERROR("Failed to create Metal device");
        return;
    }
    m_device->retain();

    m_command_queue = m_device->newCommandQueue();
    if (m_command_queue == nil)
    {
        LOG_ERROR("Failed to create Metal command queue");
        return;
    }

    m_graphics_command_list = Memory::Alloc<MetalCommandList>(m_command_queue);
}

Buffer *MetalRHI::CreateBuffer(const BufferCreateInfo &buffer_create_info)
{
    if (m_device == nil)
    {
        LOG_ERROR("Cannot create Metal buffer before renderer initialization");
        return nullptr;
    }
    return Memory::Alloc<MetalBuffer>(buffer_create_info, m_device);
}

void MetalRHI::DestroyBuffer(Buffer *buffer)
{
    Memory::Free(buffer);
}

Texture *MetalRHI::CreateTexture(const TextureCreateInfo &texture_create_info)
{
    if (m_device == nil)
    {
        LOG_ERROR("Cannot create Metal texture before renderer initialization");
        return nullptr;
    }
    const u32 max_dimension = std::max(texture_create_info.width, texture_create_info.height);
    const u32 mip_level_count =
        texture_create_info.enanble_mipmap
            ? std::min(MAX_MIP_LEVEL,
                       static_cast<u32>(std::floor(std::log2(static_cast<double>(std::max(1u, max_dimension))))) + 1)
            : 1;

    MTL::TextureDescriptor *descriptor = MTL::TextureDescriptor::alloc()->init();
    descriptor->setTextureType(MTL::TextureType2D);
    descriptor->setPixelFormat(ToMetalPixelFormat(texture_create_info.texture_format));
    descriptor->setWidth(texture_create_info.width);
    descriptor->setHeight(texture_create_info.height);
    descriptor->setDepth(texture_create_info.depth);
    descriptor->setMipmapLevelCount(mip_level_count);
    descriptor->setArrayLength(texture_create_info.array_layer);
    descriptor->setUsage(ToMetalTextureUsage(texture_create_info.descriptor_types));

    MTL::Texture *texture = m_device->newTexture(descriptor);
    descriptor->release();
    if (texture == nil)
    {
        LOG_ERROR("Failed to create Metal texture '{}'",
                  texture_create_info.debug_name != nullptr ? texture_create_info.debug_name : "");
        return nullptr;
    }
    return Memory::Alloc<MetalTexture>(texture_create_info, texture, true);
}

void MetalRHI::DestroyTexture(Texture *texture)
{
    Memory::Free(texture);
}

RenderTarget *MetalRHI::CreateRenderTarget(const RenderTargetCreateInfo &render_target_create_info)
{
    if (m_device == nil)
    {
        LOG_ERROR("Cannot create Metal render target before renderer initialization");
        return nullptr;
    }
    TextureCreateInfo texture_create_info{};
    texture_create_info.descriptor_types = render_target_create_info.rt_type == RenderTargetType::DEPTH_STENCIL
                                               ? DescriptorType::DESCRIPTOR_TYPE_DEPTH_STENCIL_ATTACHMENT
                                               : DescriptorType::DESCRIPTOR_TYPE_COLOR_ATTACHMENT;
    texture_create_info.initial_state = render_target_create_info.rt_type == RenderTargetType::DEPTH_STENCIL
                                            ? ResourceState::RESOURCE_STATE_DEPTH_WRITE
                                            : ResourceState::RESOURCE_STATE_RENDER_TARGET;
    texture_create_info.texture_type = TextureType::TEXTURE_TYPE_2D;
    texture_create_info.texture_format = render_target_create_info.rt_format;
    texture_create_info.width = render_target_create_info.width;
    texture_create_info.height = render_target_create_info.height;
    texture_create_info.depth = 1;
    texture_create_info.array_layer = 1;
    texture_create_info.debug_name = "MetalRenderTarget";

    Texture *texture = CreateTexture(texture_create_info);
    if (texture == nullptr)
    {
        return nullptr;
    }
    return Memory::Alloc<MetalRenderTarget>(texture);
}

void MetalRHI::DestroyRenderTarget(RenderTarget *render_target)
{
    Memory::Free(render_target);
}

SwapChain *MetalRHI::CreateSwapChain(const SwapChainCreateInfo &create_info)
{
    if (m_device == nil)
    {
        LOG_ERROR("Cannot create Metal swap chain before renderer initialization");
        return nullptr;
    }
    if (m_window == nullptr)
    {
        LOG_ERROR("Cannot create Metal swap chain without window");
        return nullptr;
    }

    auto *swap_chain = Memory::Alloc<MetalSwapChain>(create_info, m_window);
    if (!swap_chain->Initialize(m_device, m_window))
    {
        Memory::Free(swap_chain);
        return nullptr;
    }
    return swap_chain;
}

void MetalRHI::DestroySwapChain(SwapChain *swap_chain)
{
    auto *metal_swap_chain = static_cast<MetalSwapChain *>(swap_chain);
    for (auto *render_target : metal_swap_chain->render_targets)
    {
        Memory::Free(render_target);
    }
    metal_swap_chain->render_targets.clear();
    Memory::Free(metal_swap_chain);
}

Shader *MetalRHI::CreateShader(ShaderType type, const Path &file_name, const char *entry_point)
{
    if (m_device == nil)
    {
        LOG_ERROR("Cannot create Metal shader before renderer initialization");
        return nullptr;
    }
    auto shader_source_bytes = Path::read_file(file_name.c_str());
    if (shader_source_bytes.empty())
    {
        LOG_ERROR("Failed to read Metal shader '{}': {}", file_name.string(), "unknown error");
        return nullptr;
    }
    shader_source_bytes.push_back('\0');
    NS::String *source = NS::String::string(shader_source_bytes.data(), NS::UTF8StringEncoding);

    NS::Error *error = nil;
    MTL::CompileOptions *compile_options = MTL::CompileOptions::alloc()->init();
    MTL::Library *library = m_device->newLibrary(source, compile_options, &error);
    compile_options->release();
    if (library == nil)
    {
        LOG_ERROR("Failed to compile Metal shader '{}': {}", file_name.string(), GetNSErrorDescription(error));
        return nullptr;
    }

    MTL::Function *function = library->newFunction(ToNSString(entry_point));
    if (function == nil)
    {
        LOG_ERROR("Failed to find Metal entry point '{}' in '{}'", entry_point, file_name.string());
        library->release();
        return nullptr;
    }

    return Memory::Alloc<MetalShader>(type, entry_point, library, function);
}

void MetalRHI::DestroyShader(Shader *shader_program)
{
    Memory::Free(shader_program);
}

Pipeline *MetalRHI::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info)
{
    if (m_device == nil)
    {
        LOG_ERROR("Cannot create Metal graphics pipeline before renderer initialization");
        return nullptr;
    }
    auto *vertex_shader = static_cast<MetalShader *>(create_info.shader_program.VertexShader());
    auto *pixel_shader = static_cast<MetalShader *>(create_info.shader_program.PixelShader());
    if (vertex_shader == nullptr || pixel_shader == nullptr)
    {
        LOG_ERROR("Metal graphics pipeline requires vertex and pixel shaders");
        return nullptr;
    }

    MTL::RenderPipelineDescriptor *descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setVertexFunction(vertex_shader->m_function);
    descriptor->setFragmentFunction(pixel_shader->m_function);
    descriptor->colorAttachments()->object(0)->setPixelFormat(
        ToMetalPixelFormat(create_info.render_target_formats.color_attachment_formats[0], true));
    descriptor->setDepthAttachmentPixelFormat(
        create_info.depth_stencil_state.depth_test || create_info.depth_stencil_state.depth_write
            ? ToMetalPixelFormat(create_info.render_target_formats.depth_stencil_format)
            : MTL::PixelFormatInvalid);

    MTL::VertexDescriptor *vertex_descriptor = MTL::VertexDescriptor::alloc()->init();
    for (u32 attribute_index = 0; attribute_index < create_info.vertex_input_state.attribute_count; ++attribute_index)
    {
        const auto &attribute = create_info.vertex_input_state.attributes[attribute_index];
        vertex_descriptor->attributes()
            ->object(attribute.location)
            ->setFormat(ToMetalVertexFormat(attribute.attrib_format, attribute.portion));
        vertex_descriptor->attributes()->object(attribute.location)->setOffset(attribute.offset);
        vertex_descriptor->attributes()->object(attribute.location)->setBufferIndex(attribute.binding);
        vertex_descriptor->layouts()->object(attribute.binding)->setStride(attribute.stride);
        vertex_descriptor->layouts()
            ->object(attribute.binding)
            ->setStepFunction(attribute.input_rate == VertexInputRate::VERTEX_ATTRIB_RATE_INSTANCE
                                  ? MTL::VertexStepFunctionPerInstance
                                  : MTL::VertexStepFunctionPerVertex);
        vertex_descriptor->layouts()->object(attribute.binding)->setStepRate(1);
    }
    descriptor->setVertexDescriptor(vertex_descriptor);

    NS::Error *error = nil;
    MTL::RenderPipelineState *pipeline_state = m_device->newRenderPipelineState(descriptor, &error);
    vertex_descriptor->release();
    descriptor->release();
    if (pipeline_state == nil)
    {
        LOG_ERROR("Failed to create Metal render pipeline: {}", GetNSErrorDescription(error));
        return nullptr;
    }

    MTL::DepthStencilState *depth_stencil_state = nil;
    if (create_info.depth_stencil_state.depth_test || create_info.depth_stencil_state.depth_write)
    {
        MTL::DepthStencilDescriptor *depth_descriptor = MTL::DepthStencilDescriptor::alloc()->init();
        depth_descriptor->setDepthCompareFunction(ToMetalCompareFunction(create_info.depth_stencil_state.depth_func));
        depth_descriptor->setDepthWriteEnabled(create_info.depth_stencil_state.depth_write);
        depth_stencil_state = m_device->newDepthStencilState(depth_descriptor);
        depth_descriptor->release();
    }

    auto *pipeline = Memory::Alloc<MetalPipeline>();
    pipeline->m_render_pipeline_state = pipeline_state;
    pipeline->m_depth_stencil_state = depth_stencil_state;
    pipeline->SetTopology(create_info.input_assembly_state.topology);
    pipeline->m_primitive_type = ToMetalPrimitiveType(create_info.input_assembly_state.topology);
    pipeline->m_front_winding = ToMetalWinding(create_info.rasterization_state.front_face);
    pipeline->m_cull_mode = ToMetalCullMode(create_info.rasterization_state.cull_mode);
    pipeline->m_fill_mode = ToMetalFillMode(create_info.rasterization_state.fill_mode);
    pipeline->m_viewport = MTL::Viewport{0.0,
                                         0.0,
                                         static_cast<double>(create_info.view_port_state.width),
                                         static_cast<double>(create_info.view_port_state.height),
                                         0.0,
                                         1.0};
    return pipeline;
}

Pipeline *MetalRHI::CreateComputePipeline(const ComputePipelineCreateInfo &create_info)
{
    (void)create_info;
    LOG_ERROR("Metal compute pipeline is not implemented in Phase 1");
    return nullptr;
}

void MetalRHI::DestroyPipeline(Pipeline *pipeline)
{
    Memory::Free(pipeline);
}

CommandList *MetalRHI::GetCommandList(CommandQueueType type)
{
    (void)type;
    return m_graphics_command_list;
}

Semaphore *MetalRHI::CreateSemaphore1()
{
    return Memory::Alloc<MetalSemaphore>();
}

void MetalRHI::DestroySemaphore(Semaphore *semaphore)
{
    Memory::Free(semaphore);
}

Sampler *MetalRHI::CreateSampler(const SamplerDesc &sampler_desc)
{
    if (m_device == nil)
    {
        LOG_ERROR("Cannot create Metal sampler before renderer initialization");
        return nullptr;
    }
    MTL::SamplerDescriptor *descriptor = MTL::SamplerDescriptor::alloc()->init();
    descriptor->setMinFilter(ToMetalFilter(sampler_desc.min_filter));
    descriptor->setMagFilter(ToMetalFilter(sampler_desc.mag_filter));
    descriptor->setMipFilter(ToMetalMipFilter(sampler_desc.mip_map_mode));
    descriptor->setSAddressMode(ToMetalAddressMode(sampler_desc.address_u));
    descriptor->setTAddressMode(ToMetalAddressMode(sampler_desc.address_v));
    descriptor->setRAddressMode(ToMetalAddressMode(sampler_desc.address_w));
    descriptor->setLodMinClamp(sampler_desc.mMinLod);
    descriptor->setLodMaxClamp(sampler_desc.mMaxLod);
    descriptor->setMaxAnisotropy(
        sampler_desc.mMaxAnisotropy > 1.0f ? static_cast<NS::UInteger>(sampler_desc.mMaxAnisotropy) : 1);
    descriptor->setCompareFunction(ToMetalCompareFunction(sampler_desc.mCompareFunc));

    MTL::SamplerState *sampler_state = m_device->newSamplerState(descriptor);
    descriptor->release();
    if (sampler_state == nil)
    {
        LOG_ERROR("Failed to create Metal sampler");
        return nullptr;
    }
    return Memory::Alloc<MetalSampler>(sampler_state);
}

void MetalRHI::DestroySampler(Sampler *sampler)
{
    Memory::Free(sampler);
}

void MetalRHI::WaitGpuExecution(CommandQueueType queue_type)
{
    (void)queue_type;
    if (m_last_committed_command_buffer != nil)
    {
        m_last_committed_command_buffer->waitUntilCompleted();
        m_last_committed_command_buffer->release();
        m_last_committed_command_buffer = nil;
    }
}

void MetalRHI::ResetRHIResources()
{
}

void MetalRHI::ResetFence(CommandQueueType queue_type)
{
    (void)queue_type;
}

void MetalRHI::SubmitCommandLists(const QueueSubmitInfo &queue_submit_info)
{
    (void)queue_submit_info;
    for (auto *command_list : queue_submit_info.command_lists)
    {
        auto *metal_command_list = static_cast<MetalCommandList *>(command_list);
        if (metal_command_list->m_command_buffer == nil)
        {
            continue;
        }

        if (metal_command_list->m_present_swap_chain != nullptr)
        {
            m_pending_present_command_list = metal_command_list;
            continue;
        }

        CommitCommandBuffer(metal_command_list->m_command_buffer);
    }
}

void MetalRHI::AcquireNextFrame(SwapChain *swap_chain)
{
    auto *metal_swap_chain = static_cast<MetalSwapChain *>(swap_chain);
    metal_swap_chain->UpdateDrawableSize(m_window);

    if (metal_swap_chain->m_current_drawable != nil)
    {
        metal_swap_chain->m_current_drawable->release();
        metal_swap_chain->m_current_drawable = nil;
    }

    metal_swap_chain->m_current_drawable =
        static_cast<CA::MetalDrawable *>(metal_swap_chain->m_layer->nextDrawable()->retain());
    if (metal_swap_chain->m_current_drawable == nil)
    {
        LOG_WARN("Failed to acquire next Metal drawable");
        return;
    }

    metal_swap_chain->image_index = metal_swap_chain->m_back_buffer_count > 0
                                        ? metal_swap_chain->current_frame_index % metal_swap_chain->m_back_buffer_count
                                        : 0;
    metal_swap_chain->current_frame_index++;

    auto *render_target =
        static_cast<MetalRenderTarget *>(metal_swap_chain->render_targets[metal_swap_chain->image_index]);
    auto *texture = static_cast<MetalTexture *>(render_target->GetTexture());
    texture->SetNativeTexture(metal_swap_chain->m_current_drawable->texture());
    texture->m_state = ResourceState::RESOURCE_STATE_PRESENT;

    if (m_graphics_command_list != nullptr)
    {
        m_graphics_command_list->SetActiveSwapChain(metal_swap_chain);
    }
}

void MetalRHI::Present(const QueuePresentInfo &queue_present_info)
{
    auto *metal_swap_chain = static_cast<MetalSwapChain *>(queue_present_info.swap_chain);
    if (m_pending_present_command_list == nullptr || metal_swap_chain->m_current_drawable == nil)
    {
        LOG_WARN("Metal present skipped because command buffer or drawable is missing");
        return;
    }

    m_pending_present_command_list->m_command_buffer->presentDrawable(metal_swap_chain->m_current_drawable);
    CommitCommandBuffer(m_pending_present_command_list->m_command_buffer);
    m_pending_present_command_list = nullptr;
}

double MetalRHI::QueryResult()
{
    return 0.0;
}

void MetalRHI::CommitCommandBuffer(MTL::CommandBuffer *command_buffer) noexcept
{
    command_buffer->commit();
    if (m_last_committed_command_buffer != nil)
    {
        m_last_committed_command_buffer->release();
    }
    m_last_committed_command_buffer = static_cast<MTL::CommandBuffer *>(command_buffer->retain());
}

std::unique_ptr<RHI> CreateMetalRenderBackend(bool offscreen) noexcept
{
    return std::make_unique<MetalRHI>(offscreen);
}

} // namespace Horizon::Backend
