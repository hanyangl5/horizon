
#include <core/log.h>
#include <core/math.h>
#include <rhi/resource_barrier.h>
#include <rhi/rhi.h>
#include <scene/scene_renderer/renderer.h>

namespace TEST
{

using namespace Horizon;
using namespace Horizon::Backend;

class RHITest
{
  public:
    RHITest()
    {
        Config config{};
        config.width = width;
        config.height = height;
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.app_type = Horizon::ApplicationType::OFFSCREEN_GRAPHICS;
        renderer = std::make_unique<Renderer>(config);

        width = config.width;
        height = config.height;
    }
    ~RHITest() = default;
    u32 width = 800;
    u32 height = 600;
    std::unique_ptr<Window> window;
    std::unique_ptr<Renderer> renderer;
};

// ---- Buffer Tests ----

void BufferCreateInfoTest()
{
    BufferCreateInfo ci{};
    ci.descriptor_types = DESCRIPTOR_TYPE_CONSTANT_BUFFER;
    ci.initial_state = RESOURCE_STATE_SHADER_RESOURCE;
    ci.size = 256;
    ci.debug_name = "test_cbuffer";

    BufferCreateInfo ci_vertex{};
    ci_vertex.descriptor_types = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    ci_vertex.initial_state = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    ci_vertex.size = 1024;
    ci_vertex.debug_name = "test_vbuffer";

    BufferCreateInfo ci_index{};
    ci_index.descriptor_types = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ci_index.initial_state = RESOURCE_STATE_INDEX_BUFFER;
    ci_index.size = 512;
    ci_index.debug_name = "test_ibuffer";

    BufferCreateInfo ci_uav{};
    ci_uav.descriptor_types = DESCRIPTOR_TYPE_RW_BUFFER;
    ci_uav.initial_state = RESOURCE_STATE_UNORDERED_ACCESS;
    ci_uav.size = 4096;
    ci_uav.debug_name = "test_uav_buffer";

    BufferCreateInfo ci_indirect{};
    ci_indirect.descriptor_types = DESCRIPTOR_TYPE_INDIRECT_BUFFER;
    ci_indirect.initial_state = RESOURCE_STATE_INDIRECT_ARGUMENT;
    ci_indirect.size = sizeof(DrawIndexedInstancedCommand);
    ci_indirect.debug_name = "test_indirect_buffer";
}

void BufferTest(RHITest *rhi_test)
{
    auto *rhi = rhi_test->renderer->GetRhi();

    Buffer *cbuffer = rhi->CreateBuffer(
        {DESCRIPTOR_TYPE_CONSTANT_BUFFER, RESOURCE_STATE_SHADER_RESOURCE, 256, "test_cbuffer"});

    Buffer *vbuffer = rhi->CreateBuffer(
        {DESCRIPTOR_TYPE_VERTEX_BUFFER, RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 1024, "test_vbuffer"});

    Buffer *ibuffer =
        rhi->CreateBuffer({DESCRIPTOR_TYPE_INDEX_BUFFER, RESOURCE_STATE_INDEX_BUFFER, 512, "test_ibuffer"});

    Buffer *uav_buffer =
        rhi->CreateBuffer({DESCRIPTOR_TYPE_RW_BUFFER, RESOURCE_STATE_UNORDERED_ACCESS, 4096, "test_uav"});

    (void)cbuffer->m_debug_name;
    (void)cbuffer->m_descriptor_types;
    (void)cbuffer->m_resource_state;
    (void)cbuffer->m_size;

    rhi->DestroyBuffer(cbuffer);
    rhi->DestroyBuffer(vbuffer);
    rhi->DestroyBuffer(ibuffer);
    rhi->DestroyBuffer(uav_buffer);
}

// ---- Texture Tests ----

void TextureCreateInfoTest()
{
    TextureCreateInfo ci_2d{};
    ci_2d.descriptor_types = DESCRIPTOR_TYPE_TEXTURE;
    ci_2d.initial_state = RESOURCE_STATE_SHADER_RESOURCE;
    ci_2d.texture_type = TextureType::TEXTURE_TYPE_2D;
    ci_2d.texture_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
    ci_2d.width = 512;
    ci_2d.height = 512;
    ci_2d.depth = 1;
    ci_2d.enanble_mipmap = true;
    ci_2d.array_layer = 1;
    ci_2d.debug_name = "test_texture_2d";

    TextureCreateInfo ci_cube{};
    ci_cube.descriptor_types = DESCRIPTOR_TYPE_TEXTURE_CUBE;
    ci_cube.initial_state = RESOURCE_STATE_SHADER_RESOURCE;
    ci_cube.texture_type = TextureType::TEXTURE_TYPE_CUBE;
    ci_cube.texture_format = TextureFormat::TEXTURE_FORMAT_RGBA16_SFLOAT;
    ci_cube.width = 256;
    ci_cube.height = 256;
    ci_cube.depth = 1;
    ci_cube.array_layer = 6;
    ci_cube.debug_name = "test_cubemap";

    TextureCreateInfo ci_depth{};
    ci_depth.descriptor_types = DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES;
    ci_depth.initial_state = RESOURCE_STATE_DEPTH_WRITE;
    ci_depth.texture_type = TextureType::TEXTURE_TYPE_2D;
    ci_depth.texture_format = TextureFormat::TEXTURE_FORMAT_D32_SFLOAT;
    ci_depth.width = 1920;
    ci_depth.height = 1080;
    ci_depth.depth = 1;
    ci_depth.debug_name = "test_depth";

    TextureCreateInfo ci_rw{};
    ci_rw.descriptor_types = DESCRIPTOR_TYPE_RW_TEXTURE;
    ci_rw.initial_state = RESOURCE_STATE_UNORDERED_ACCESS;
    ci_rw.texture_type = TextureType::TEXTURE_TYPE_2D;
    ci_rw.texture_format = TextureFormat::TEXTURE_FORMAT_R32_SFLOAT;
    ci_rw.width = 64;
    ci_rw.height = 64;
    ci_rw.depth = 1;
    ci_rw.debug_name = "test_rw_texture";

    TextureCreateInfo ci_3d{};
    ci_3d.texture_type = TextureType::TEXTURE_TYPE_3D;
    ci_3d.texture_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
    ci_3d.width = 32;
    ci_3d.height = 32;
    ci_3d.depth = 32;
    ci_3d.debug_name = "test_3d_texture";
}

void TextureTest(RHITest *rhi_test)
{
    auto *rhi = rhi_test->renderer->GetRhi();

    TextureCreateInfo ci{};
    ci.descriptor_types = DESCRIPTOR_TYPE_TEXTURE;
    ci.initial_state = RESOURCE_STATE_SHADER_RESOURCE;
    ci.texture_type = TextureType::TEXTURE_TYPE_2D;
    ci.texture_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
    ci.width = 128;
    ci.height = 128;
    ci.depth = 1;
    ci.debug_name = "test_tex";

    Texture *tex = rhi->CreateTexture(ci);

    (void)tex->m_debug_name;
    (void)tex->m_descriptor_types;
    (void)tex->m_state;
    (void)tex->m_type;
    (void)tex->m_format;
    (void)tex->m_width;
    (void)tex->m_height;
    (void)tex->m_depth;
    (void)tex->m_array_layer;
    (void)tex->mip_map_level;
    (void)tex->m_byte_per_pixel;

    rhi->DestroyTexture(tex);
}

// ---- RenderTarget Tests ----

void RenderTargetCreateInfoTest()
{
    RenderTargetCreateInfo ci_color{};
    ci_color.rt_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
    ci_color.rt_type = RenderTargetType::COLOR;
    ci_color.width = 1920;
    ci_color.height = 1080;

    RenderTargetCreateInfo ci_depth{};
    ci_depth.rt_format = TextureFormat::TEXTURE_FORMAT_D32_SFLOAT;
    ci_depth.rt_type = RenderTargetType::DEPTH_STENCIL;
    ci_depth.width = 1920;
    ci_depth.height = 1080;
}

void RenderTargetTest(RHITest *rhi_test)
{
    auto *rhi = rhi_test->renderer->GetRhi();

    RenderTarget *color_rt =
        rhi->CreateRenderTarget({TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, RenderTargetType::COLOR, 256, 256});

    RenderTarget *depth_rt = rhi->CreateRenderTarget(
        {TextureFormat::TEXTURE_FORMAT_D32_SFLOAT, RenderTargetType::DEPTH_STENCIL, 256, 256});

    Texture *color_tex = color_rt->GetTexture();
    (void)color_tex;

    rhi->DestroyRenderTarget(color_rt);
    rhi->DestroyRenderTarget(depth_rt);
}

// ---- Sampler Tests ----

void SamplerDescTest()
{
    SamplerDesc linear_clamp{};
    linear_clamp.min_filter = FilterType::FILTER_LINEAR;
    linear_clamp.mag_filter = FilterType::FILTER_LINEAR;
    linear_clamp.mip_map_mode = MipMapMode::MIPMAP_MODE_LINEAR;
    linear_clamp.address_u = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;
    linear_clamp.address_v = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;
    linear_clamp.address_w = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;
    linear_clamp.mMipLodBias = 0.0f;
    linear_clamp.mSetLodRange = false;
    linear_clamp.mMinLod = 0.0f;
    linear_clamp.mMaxLod = 12.0f;
    linear_clamp.mMaxAnisotropy = 1.0f;
    linear_clamp.mCompareFunc = CompareFunc::NEVER;

    SamplerDesc nearest_repeat{};
    nearest_repeat.min_filter = FilterType::FILTER_NEAREST;
    nearest_repeat.mag_filter = FilterType::FILTER_NEAREST;
    nearest_repeat.mip_map_mode = MipMapMode::MIPMAP_MODE_NEAREST;
    nearest_repeat.address_u = AddressMode::ADDRESS_MODE_REPEAT;
    nearest_repeat.address_v = AddressMode::ADDRESS_MODE_REPEAT;
    nearest_repeat.address_w = AddressMode::ADDRESS_MODE_REPEAT;
    nearest_repeat.mCompareFunc = CompareFunc::ALWAYS;

    SamplerDesc shadow_sampler{};
    shadow_sampler.min_filter = FilterType::FILTER_LINEAR;
    shadow_sampler.mag_filter = FilterType::FILTER_LINEAR;
    shadow_sampler.mip_map_mode = MipMapMode::MIPMAP_MODE_NEAREST;
    shadow_sampler.address_u = AddressMode::ADDRESS_MODE_CLAMP_TO_BORDER;
    shadow_sampler.address_v = AddressMode::ADDRESS_MODE_CLAMP_TO_BORDER;
    shadow_sampler.address_w = AddressMode::ADDRESS_MODE_CLAMP_TO_BORDER;
    shadow_sampler.mCompareFunc = CompareFunc::L_EQUAL;
}

void SamplerTest(RHITest *rhi_test)
{
    auto *rhi = rhi_test->renderer->GetRhi();

    SamplerDesc desc{};
    desc.min_filter = FilterType::FILTER_LINEAR;
    desc.mag_filter = FilterType::FILTER_LINEAR;
    desc.mip_map_mode = MipMapMode::MIPMAP_MODE_LINEAR;
    desc.address_u = AddressMode::ADDRESS_MODE_REPEAT;
    desc.address_v = AddressMode::ADDRESS_MODE_REPEAT;
    desc.address_w = AddressMode::ADDRESS_MODE_REPEAT;
    desc.mMipLodBias = 0.0f;
    desc.mSetLodRange = false;
    desc.mMinLod = 0.0f;
    desc.mMaxLod = 12.0f;
    desc.mMaxAnisotropy = 16.0f;
    desc.mCompareFunc = CompareFunc::NEVER;

    Sampler *sampler = rhi->CreateSampler(desc);

    rhi->DestroySampler(sampler);
}

// ---- Shader Tests ----

void ShaderCreateInfoTest()
{
    (void)ShaderType::VERTEX_SHADER;
    (void)ShaderType::PIXEL_SHADER;
    (void)ShaderType::GEOMETRY_SHADER;
    (void)ShaderType::DOMAIN_SHADER;
    (void)ShaderType::HULL_SHADER;
    (void)ShaderType::COMPUTE_SHADER;
    (void)ShaderType::MESH_SHADER;
    (void)ShaderType::RAY_GENERATION_SHADER;
    (void)ShaderType::RAY_CLOSEST_HIT_SHADER;
    (void)ShaderType::RAY_MISS_SHADER;
    (void)ShaderType::RAY_ANY_HIT_SHADER;

    ShaderStageFlags flags = GetShaderStageFlagsFromShaderType(ShaderType::VERTEX_SHADER);
    (void)flags;
    flags = GetShaderStageFlagsFromShaderType(ShaderType::PIXEL_SHADER);
    flags = GetShaderStageFlagsFromShaderType(ShaderType::COMPUTE_SHADER);
}

// ---- Pipeline Tests ----

void PipelineCreateInfoTest()
{
    GraphicsPipelineCreateInfo gpci{};

    gpci.vertex_input_state.attribute_count = 2;
    gpci.vertex_input_state.attributes[0] = {VertexAttribFormat::F32, 3, VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX,
                                              0, 0, sizeof(f32) * 3, 0, "POSITION", 0};
    gpci.vertex_input_state.attributes[1] = {VertexAttribFormat::F32, 2, VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX,
                                              1, 0, sizeof(f32) * 2, sizeof(f32) * 3, "TEXCOORD", 0};

    gpci.input_assembly_state.topology = PrimitiveTopology::TRIANGLE_LIST;

    gpci.view_port_state.width = 1920;
    gpci.view_port_state.height = 1080;

    gpci.rasterization_state.front_face = FrontFace::CCW;
    gpci.rasterization_state.cull_mode = CullMode::BACK;
    gpci.rasterization_state.fill_mode = FillMode::TRIANGLE;
    gpci.rasterization_state.discard = false;

    gpci.depth_stencil_state.depth_test = true;
    gpci.depth_stencil_state.depth_write = true;
    gpci.depth_stencil_state.stencil_enabled = false;
    gpci.depth_stencil_state.depth_func = CompareFunc::LESS;
    gpci.depth_stencil_state.depth_stencil_format = TextureFormat::TEXTURE_FORMAT_D32_SFLOAT;
    gpci.depth_stencil_state.depthNear = 0.0f;
    gpci.depth_stencil_state.depthFar = 1.0f;

    gpci.multi_sample_state.sample_count = 1;

    gpci.render_target_formats.color_attachment_count = 1;
    gpci.render_target_formats.color_attachment_formats.push_back(TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM);
    gpci.render_target_formats.has_depth = true;
    gpci.render_target_formats.depth_stencil_format = TextureFormat::TEXTURE_FORMAT_D32_SFLOAT;

    ShaderPrograms programs{};
    programs.SetShader(ShaderType::VERTEX_SHADER, nullptr);
    programs.SetShader(ShaderType::PIXEL_SHADER, nullptr);
    gpci.shader_program = programs;
    (void)programs.VertexShader();
    (void)programs.PixelShader();
    (void)programs.ComputeShader();
    (void)programs.GeometryShader();
    (void)programs.DomainShader();
    (void)programs.HullShader();
    (void)programs.MeshShader();

    ComputePipelineCreateInfo cpci{};
    cpci.shader_program.SetShader(ShaderType::COMPUTE_SHADER, nullptr);
}

// ---- Semaphore Tests ----

void SemaphoreTest(RHITest *rhi_test)
{
    auto *rhi = rhi_test->renderer->GetRhi();

    Semaphore *sem = rhi->CreateSemaphore1();

    sem->AddWaitStage(CommandQueueType::GRAPHICS);
    u32 wait_stage = sem->GetWaitStage();
    (void)wait_stage;

    rhi->DestroySemaphore(sem);
}

// ---- CommandList Tests ----

void CommandListTest(RHITest *rhi_test)
{
    auto *rhi = rhi_test->renderer->GetRhi();

    Buffer *buffer =
        rhi->CreateBuffer({DESCRIPTOR_TYPE_CONSTANT_BUFFER, RESOURCE_STATE_SHADER_RESOURCE, sizeof(Math::float3),
                           "test_cmd_buffer"});

    Math::float3 data{static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                      static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                      static_cast<float>(rand()) / static_cast<float>(RAND_MAX)};

    CommandList *transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);

    transfer->BeginRecording();
    transfer->UpdateBuffer(buffer, &data, sizeof(data));
    transfer->EndRecording();

    QueueSubmitInfo submit_info{};
    submit_info.queue_type = CommandQueueType::TRANSFER;
    submit_info.command_lists = {transfer};
    rhi->SubmitCommandLists(submit_info);
    rhi->WaitGpuExecution(CommandQueueType::TRANSFER);

    rhi->DestroyBuffer(buffer);
}

void CommandListGraphicsAPITest(RHITest *rhi_test)
{
    auto *rhi = rhi_test->renderer->GetRhi();

    CommandList *graphics_cmd = rhi->GetCommandList(CommandQueueType::GRAPHICS);
    CommandList *compute_cmd = rhi->GetCommandList(CommandQueueType::COMPUTE);

    (void)graphics_cmd;
    (void)compute_cmd;
}

// ---- Resource Barrier Tests ----

void BarrierDescTest(RHITest *rhi_test)
{
    auto *rhi = rhi_test->renderer->GetRhi();

    Buffer *buffer =
        rhi->CreateBuffer({DESCRIPTOR_TYPE_RW_BUFFER, RESOURCE_STATE_UNORDERED_ACCESS, 256, "test_barrier_buf"});

    TextureCreateInfo tex_ci{};
    tex_ci.descriptor_types = DESCRIPTOR_TYPE_TEXTURE;
    tex_ci.initial_state = RESOURCE_STATE_SHADER_RESOURCE;
    tex_ci.texture_type = TextureType::TEXTURE_TYPE_2D;
    tex_ci.texture_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
    tex_ci.width = 64;
    tex_ci.height = 64;
    tex_ci.depth = 1;
    tex_ci.debug_name = "test_barrier_tex";

    Texture *texture = rhi->CreateTexture(tex_ci);

    BufferBarrierDesc buf_barrier{};
    buf_barrier.buffer = buffer;
    buf_barrier.src_state = RESOURCE_STATE_UNORDERED_ACCESS;
    buf_barrier.dst_state = RESOURCE_STATE_SHADER_RESOURCE;
    buf_barrier.queue_op = QueueOp::IGNORED;

    TextureBarrierDesc tex_barrier{};
    tex_barrier.texture = texture;
    tex_barrier.src_state = RESOURCE_STATE_SHADER_RESOURCE;
    tex_barrier.dst_state = RESOURCE_STATE_RENDER_TARGET;
    tex_barrier.first_mip_level = 0;
    tex_barrier.mip_level_count = 1;
    tex_barrier.first_layer = 0;
    tex_barrier.layer_count = 1;
    tex_barrier.queue_op = QueueOp::IGNORED;

    BarrierDesc barrier{};
    barrier.buffer_memory_barriers.push_back(buf_barrier);
    barrier.texture_memory_barriers.push_back(tex_barrier);

    rhi->DestroyTexture(texture);
    rhi->DestroyBuffer(buffer);
}

// ---- QueueSubmitInfo Tests ----

void QueueSubmitInfoTest(RHITest *rhi_test)
{
    auto *rhi = rhi_test->renderer->GetRhi();

    Semaphore *wait_sem = rhi->CreateSemaphore1();
    Semaphore *signal_sem = rhi->CreateSemaphore1();

    CommandList *cmd = rhi->GetCommandList(CommandQueueType::GRAPHICS);

    QueueSubmitInfo info{};
    info.queue_type = CommandQueueType::GRAPHICS;
    info.command_lists = {cmd};
    info.wait_semaphores = {wait_sem};
    info.signal_semaphores = {signal_sem};
    info.wait_image_acquired = false;
    info.signal_render_complete = false;

    rhi->DestroySemaphore(wait_sem);
    rhi->DestroySemaphore(signal_sem);
}

// ---- Enum Utility Tests ----

void EnumUtilTest()
{
    u32 stride = GetStrideFromVertexAttributeDescription(VertexAttribFormat::F32, 3);
    (void)stride;
    stride = GetStrideFromVertexAttributeDescription(VertexAttribFormat::F16, 4);
    stride = GetStrideFromVertexAttributeDescription(VertexAttribFormat::U8, 4);
    stride = GetStrideFromVertexAttributeDescription(VertexAttribFormat::UN16, 2);

    u32 bytes = GetBytesFromTextureFormat(TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM);
    (void)bytes;
    bytes = GetBytesFromTextureFormat(TextureFormat::TEXTURE_FORMAT_D32_SFLOAT);
    bytes = GetBytesFromTextureFormat(TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT);
    bytes = GetBytesFromTextureFormat(TextureFormat::TEXTURE_FORMAT_R16_SFLOAT);

    ShaderStageFlags vs_flag = GetShaderStageFlagsFromShaderType(ShaderType::VERTEX_SHADER);
    ShaderStageFlags ps_flag = GetShaderStageFlagsFromShaderType(ShaderType::PIXEL_SHADER);
    ShaderStageFlags cs_flag = GetShaderStageFlagsFromShaderType(ShaderType::COMPUTE_SHADER);
    (void)vs_flag;
    (void)ps_flag;
    (void)cs_flag;
}

// ---- RenderPassBeginInfo Test ----

void RenderPassBeginInfoTest()
{
    RenderPassBeginInfo rpbi{};
    rpbi.render_target_count = 1;
    rpbi.render_targets[0].data = nullptr;
    ClearColorValue ccv{};
    ccv.float32[0] = 0.0f;
    ccv.float32[1] = 0.0f;
    ccv.float32[2] = 0.0f;
    ccv.float32[3] = 1.0f;
    rpbi.render_targets[0].clear_color = ccv;
    rpbi.render_targets[0].load_op = RenderTargetLoadOp::CLEAR;
    rpbi.render_targets[0].store_op = RenderTargetStoreOp::STORE;

    rpbi.depth_stencil.data = nullptr;
    ClearValueDepthStencil depth_clear{};
    depth_clear.depth = 1.0f;
    depth_clear.stencil = 0;
    rpbi.depth_stencil.clear_color = depth_clear;
    rpbi.depth_stencil.load_op = RenderTargetLoadOp::CLEAR;
    rpbi.depth_stencil.store_op = RenderTargetStoreOp::DONT_CARE;

    rpbi.render_area = {0, 0, 1920, 1080};
    rpbi.debug_name = "test_pass";
}

// ---- DrawCommand Tests ----

void DrawCommandTest()
{
    DrawParam dp{};
    dp.indexCount = 36;
    dp.instanceCount = 1;
    dp.firstIndex = 0;
    dp.vertexOffset = 0;
    dp.firstInstance = 0;
    (void)dp;

    DrawIndexedInstancedCommand diic{};
    diic.index_count = 36;
    diic.instance_count = 1;
    diic.first_index = 0;
    diic.vertex_offset = 0;
    diic.first_instance = 0;
    (void)diic;
}

// ---- TextureDataDesc / UpdateDesc Tests ----

void TextureDataDescTest()
{
    TextureDataDesc tdd{};
    tdd.width = 256;
    tdd.height = 256;
    tdd.depth = 1;
    tdd.layer_count = 1;
    tdd.mipmap_count = 1;
    tdd.format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
    tdd.type = TextureType::TEXTURE_TYPE_2D;
    tdd.raw_data.resize(256 * 256 * 4, 0);

    TextureUpdateDesc tud{};
    tud.size = 256 * 256 * 4;
    tud.first_mip_level = 0;
    tud.mip_level_count = 1;
    tud.first_layer = 0;
    tud.layer_count = 1;
    tud.texture_data_desc = &tdd;

    BufferUpdateDesc bud{};
    bud.data = nullptr;
    bud.size = 1024;
    bud.offset = 0;
    (void)bud;
}

// ---- Descriptor/RootSignature Tests ----

void DescriptorDescTest()
{
    DescriptorDesc dd{};
    dd.type = DESCRIPTOR_TYPE_CONSTANT_BUFFER;
    dd.vk_binding = 0;

    PushConstantDesc pcd{};
    pcd.size = 64;
    pcd.offset = 0;
    pcd.shader_stages = SHADER_STAGE_VERTEX_SHADER | SHADER_STAGE_PIXEL_SHADER;

    RootSignatureDesc rsd{};
    rsd.descriptors[0]["cbuffer0"] = dd;
    rsd.push_constants["push_data"] = pcd;

    VkPipelineLayoutDesc pld{};
    pld.descriptor_set_hash_key = 0;
    pld.bindless_descriptor_set_hash_key = 0;
    (void)pld;
}

} // namespace TEST

int main()
{
    using namespace Horizon;

    LOG_INFO("=== RHI Compile Test Suite ===");

    // Offline tests (no GPU needed)
    TEST::BufferCreateInfoTest();
    TEST::TextureCreateInfoTest();
    TEST::RenderTargetCreateInfoTest();
    TEST::SamplerDescTest();
    TEST::ShaderCreateInfoTest();
    TEST::PipelineCreateInfoTest();
    TEST::EnumUtilTest();
    TEST::RenderPassBeginInfoTest();
    TEST::DrawCommandTest();
    TEST::TextureDataDescTest();
    TEST::DescriptorDescTest();

    LOG_INFO("=== Offline struct/enum tests passed ===");

    // GPU tests (need a Vulkan device)
    TEST::RHITest rhi_test;

    TEST::BufferTest(&rhi_test);
    LOG_INFO("BufferTest passed");

    TEST::TextureTest(&rhi_test);
    LOG_INFO("TextureTest passed");

    TEST::RenderTargetTest(&rhi_test);
    LOG_INFO("RenderTargetTest passed");

    TEST::SamplerTest(&rhi_test);
    LOG_INFO("SamplerTest passed");

    TEST::SemaphoreTest(&rhi_test);
    LOG_INFO("SemaphoreTest passed");

    TEST::CommandListTest(&rhi_test);
    LOG_INFO("CommandListTest passed");

    TEST::CommandListGraphicsAPITest(&rhi_test);
    LOG_INFO("CommandListGraphicsAPITest passed");

    TEST::BarrierDescTest(&rhi_test);
    LOG_INFO("BarrierDescTest passed");

    TEST::QueueSubmitInfoTest(&rhi_test);
    LOG_INFO("QueueSubmitInfoTest passed");

    LOG_INFO("=== All RHI tests passed ===");
    return 0;
}
