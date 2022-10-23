#include "deferred.h"

DeferredData::DeferredData(Backend::RHI *rhi) noexcept {

    // geometry pass
    {
        gbuffer0 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_SNORM,
                                                                  RenderTargetType::COLOR, _width, _height});
        gbuffer1 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                  RenderTargetType::COLOR, _width, _height});
        gbuffer2 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                  RenderTargetType::COLOR, _width, _height});
        gbuffer3 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                  RenderTargetType::COLOR, _width, _height});

        depth = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_D32_SFLOAT,
                                                               RenderTargetType::DEPTH_STENCIL, _width, _height});

        graphics_pass_ci.vertex_input_state.attribute_count = 5;

        auto &pos = graphics_pass_ci.vertex_input_state.attributes[0];
        pos.attrib_format = VertexAttribFormat::F32; // position
        pos.portion = 3;
        pos.binding = 0;
        pos.location = 0;
        pos.offset = 0;
        pos.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;

        auto &normal = graphics_pass_ci.vertex_input_state.attributes[1];
        normal.attrib_format = VertexAttribFormat::F32; // normal, TOOD: SN16 is a better format
        normal.portion = 3;
        normal.binding = 0;
        normal.location = 1;
        normal.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        normal.offset = offsetof(Vertex, normal);

        auto &uv0 = graphics_pass_ci.vertex_input_state.attributes[2];
        uv0.attrib_format = VertexAttribFormat::F32; // uv0 TOOD: UN16 is a better format
        uv0.portion = 2;
        uv0.binding = 0;
        uv0.location = 2;
        uv0.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv0.offset = offsetof(Vertex, uv0);

        auto &uv1 = graphics_pass_ci.vertex_input_state.attributes[3];
        uv1.attrib_format = VertexAttribFormat::F32; // uv1 TOOD: UN16 is a better format
        uv1.portion = 2;
        uv1.binding = 0;
        uv1.location = 3;
        uv1.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv1.offset = offsetof(Vertex, uv1);

        auto &tangent = graphics_pass_ci.vertex_input_state.attributes[4];
        tangent.attrib_format = VertexAttribFormat::F32;
        tangent.portion = 3;
        tangent.binding = 0;
        tangent.location = 4;
        tangent.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        tangent.offset = offsetof(Vertex, tangent);

        graphics_pass_ci.view_port_state.width = _width;
        graphics_pass_ci.view_port_state.height = _height;

        graphics_pass_ci.depth_stencil_state.depth_func = DepthFunc::LESS;
        graphics_pass_ci.depth_stencil_state.depthNear = 0.0f;
        graphics_pass_ci.depth_stencil_state.depthNear = 1.0f;
        graphics_pass_ci.depth_stencil_state.depth_test = true;
        graphics_pass_ci.depth_stencil_state.depth_write = true;
        graphics_pass_ci.depth_stencil_state.stencil_enabled = false;

        graphics_pass_ci.input_assembly_state.topology = PrimitiveTopology::TRIANGLE_LIST;

        graphics_pass_ci.multi_sample_state.sample_count = 1;

        graphics_pass_ci.rasterization_state.cull_mode = CullMode::BACK;
        graphics_pass_ci.rasterization_state.discard = false;
        graphics_pass_ci.rasterization_state.fill_mode = FillMode::TRIANGLE;
        graphics_pass_ci.rasterization_state.front_face = FrontFace::CCW;

        graphics_pass_ci.render_target_formats.color_attachment_count = 4;
        graphics_pass_ci.render_target_formats.color_attachment_formats =
            std::vector<TextureFormat>{gbuffer0->GetTexture()->m_format, gbuffer1->GetTexture()->m_format,
                                       gbuffer2->GetTexture()->m_format, gbuffer3->GetTexture()->m_format};
        graphics_pass_ci.render_target_formats.has_depth = true;
        graphics_pass_ci.render_target_formats.depth_stencil_format = depth->GetTexture()->m_format;

        geometry_pass = rhi->CreateGraphicsPipeline(graphics_pass_ci);
    }

    {
        geometry_vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, asset_path / "shaders/gbuffer.vert.hsl");

        geometry_ps = rhi->CreateShader(ShaderType::PIXEL_SHADER, 0, asset_path / "shaders/gbuffer.frag.hsl");

        shading_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, 0, asset_path / "shaders/deferred_shading.comp.hsl");
    }

    {
        deferred_shading_constants_buffer = rhi->CreateBuffer(
            BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                             ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(DeferredShadingConstants)});

        diffuse_irradiance_sh3_buffer = rhi->CreateBuffer(
            BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                             ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(DiffuseIrradianceSH3)});
    }

    // SHADING PASS
    shading_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});
    shading_color_image = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA16_SFLOAT, _width, _height, 1, false});
    {

        // ibl

        diffuse_irradiance_sh3_constants.sh = {
            Math::float4{0.474948436021805f, 0.480556935071945f, 0.501606047153473f, 0.0f},
            Math::float4{0.351380050182343f, 0.371190130710602f, 0.468852639198303f, 0.0f},
            Math::float4{0.224167078733444f, 0.204228252172470f, 0.201337367296219f, 0.0f},
            Math::float4{0.076594874262810f, -0.067332319915295f, -0.062507018446922f, 0.0f},
            Math::float4{0.061358224600554f, -0.054993789643049f, -0.051467958837748f, 0.0f},
            Math::float4{0.172064036130905f, 0.162797480821609f, 0.160090863704681f, 0.0f},
            Math::float4{0.035116069018841f, 0.028756374493241f, 0.022267108783126f, 0.0f},
            Math::float4{0.056562144309282f, -0.050596039742231f, -0.045287083834410f, 0.0f},
            Math::float4{0.004470882471651f, -0.008455731905997f, -0.017041536048055f, 0.0f}};

        prefilered_irradiance_env_map_data = TextureLoader::Load(asset_path / "envrionment/hdr29/aSpecularHDR.dds");
        
        {
            TextureCreateInfo texture_create_info{};
            texture_create_info.width = prefilered_irradiance_env_map_data.width;
            texture_create_info.height = prefilered_irradiance_env_map_data.height;
            texture_create_info.array_layer = prefilered_irradiance_env_map_data.layer_count;
            texture_create_info.enanble_mipmap = true;
            texture_create_info.texture_type = TextureType::TEXTURE_TYPE_CUBE;
            texture_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_TEXTURE_CUBE;
            texture_create_info.initial_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
            texture_create_info.texture_format = prefilered_irradiance_env_map_data.format;
            prefiltered_irradiance_env_map = rhi->CreateTexture(texture_create_info);
        }
        brdf_lut_data_desc = TextureLoader::Load(asset_path / "envrionment/hdr29/aBrdf.dds");
        {
            TextureCreateInfo texture_create_info{};
            texture_create_info.width = brdf_lut_data_desc.width;
            texture_create_info.height = brdf_lut_data_desc.height;
            texture_create_info.array_layer = brdf_lut_data_desc.layer_count;
            texture_create_info.enanble_mipmap = false;
            texture_create_info.texture_type = TextureType::TEXTURE_TYPE_2D;
            texture_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_TEXTURE_CUBE;
            texture_create_info.initial_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
            texture_create_info.texture_format = brdf_lut_data_desc.format;
            brdf_lut = rhi->CreateTexture(texture_create_info);
        }
    }

    SamplerDesc sampler_desc{};
    sampler_desc.min_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mag_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mip_map_mode = MipMapMode::MIPMAP_MODE_LINEAR;
    sampler_desc.address_u = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.address_v = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.address_w = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;

    ibl_sampler = rhi->GetSampler(sampler_desc);

    deferred_shading_constants.width = _width;
    deferred_shading_constants.height = _height;
    deferred_shading_constants.ibl_intensity = 10000.0;

    geometry_pass->SetGraphicsShader(geometry_vs, geometry_ps);
    shading_pass->SetComputeShader(shading_cs);
}
