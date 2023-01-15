#include "shading.h"

ShadingData::ShadingData(RHI *rhi) noexcept {

    { shading_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, 0, asset_path / "shaders/shading.comp.hsl"); }

    {

        diffuse_irradiance_sh3_buffer = rhi->CreateBuffer(
            BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                             ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(DiffuseIrradianceSH3)});
    }

    // SHADING PASS
    shading_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});
    shading_color_texture = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA16_SFLOAT, _width, _height, 1, false});
    {

        // ibl

        diffuse_irradiance_sh3_constants.sh = {
            Math::float4{0.473198890686035, 0.519405245780945, 0.554664373397827, 0.0f},
            Math::float4{0.416269570589066, 0.466901600360870, 0.595043838024139, 0.0f},
            Math::float4{0.070390045642853, 0.072113677859306, 0.075183071196079, 0.0f},
            Math::float4{0.200731590390205, -0.189936503767967, -0.178353592753410, 0.0f},
            Math::float4{0.165346711874008, -0.156177446246147, -0.144699439406395, 0.0f},
            Math::float4{0.037444319576025, 0.041276078671217, 0.046160303056240, 0.0f},
            Math::float4{0.007342631462961, -0.009751657955348, -0.015737744048238, 0.0f},
            Math::float4{0.023010414093733, -0.011694960296154, 0.001283747726120, 0.0f},
            Math::float4{0.000401695695473, -0.013503036461771, -0.041937090456486, 0.0f}};

        prefilered_irradiance_env_map_data =
            TextureLoader::Load(asset_path / "envrionment/football/footballSpecularHDR.dds");

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
        brdf_lut_data_desc = TextureLoader::Load(asset_path / "envrionment/football/footballBrdf.dds");
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

    ibl_sampler = rhi->CreateSampler(sampler_desc);

    shading_pass->SetComputeShader(shading_cs);

}

ShadingData::~ShadingData() noexcept {}
