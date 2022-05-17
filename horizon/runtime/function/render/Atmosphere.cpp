#include "Atmosphere.h"

#include <runtime/core/path/Path.h>
#include <runtime/function/render/rhi/VulkanEnums.h>
#include <runtime/function/render/RenderContext.h>

namespace Horizon
{
	Atmosphere::Atmosphere(
		std::shared_ptr<PipelineManager> _pipeline_manager,
		std::shared_ptr<Device> _device,
		std::shared_ptr<CommandBuffer> command_buffer,
		RenderContext& _render_context) noexcept
	{

		CreateResources(_device, command_buffer);

		// transmittance lut

		ComputePipelineCreateInfo transmittance_lut_create_info;
		transmittance_lut_create_info.name = "transmittance_lut";
		transmittance_lut_create_info.cs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("atmosphere/transmittance_lut.comp.spv"));
		transmittance_lut_create_info.descriptor_layouts = transmittance_lut_descriptor_set_layouts;
		transmittance_lut_create_info.group_count_x = 256 / 8;
		transmittance_lut_create_info.group_count_y = 64 / 8;
		transmittance_lut_create_info.group_count_z = 1;

		m_transmittance_lut_pass = _pipeline_manager->CreateComputePipeline(transmittance_lut_create_info);

		// direct irradiance lut

		ComputePipelineCreateInfo direct_irradiance_lut_create_info;
		direct_irradiance_lut_create_info.name = "direct_irradiance_lut";
		direct_irradiance_lut_create_info.cs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("atmosphere/direct_irradiance_lut.comp.spv"));
		direct_irradiance_lut_create_info.descriptor_layouts = direct_irradiance_lut_descriptor_set_layouts;
		direct_irradiance_lut_create_info.group_count_x = 64 / 8;
		direct_irradiance_lut_create_info.group_count_y = 16 / 8;
		direct_irradiance_lut_create_info.group_count_z = 1;

		m_direct_irradiance_lut_pass = _pipeline_manager->CreateComputePipeline(direct_irradiance_lut_create_info);

		// single scattering lut

		ComputePipelineCreateInfo single_scattering_lut_create_info;
		single_scattering_lut_create_info.name = "single_scattering_lut";
		single_scattering_lut_create_info.cs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("atmosphere/single_scattering_lut.comp.spv"));
		single_scattering_lut_create_info.descriptor_layouts = single_scattering_lut_descriptor_set_layouts;
		single_scattering_lut_create_info.group_count_x = 256 / 4;
		single_scattering_lut_create_info.group_count_y = 128 / 4;
		single_scattering_lut_create_info.group_count_z = 32 / 4;

		m_single_scattering_lut_pass = _pipeline_manager->CreateComputePipeline(single_scattering_lut_create_info);

		// scattering density

		ComputePipelineCreateInfo scattering_density_lut_create_info;
		scattering_density_lut_create_info.name = "scattering_density_lut";
		scattering_density_lut_create_info.cs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("atmosphere/scattering_density.comp.spv"));
		scattering_density_lut_create_info.descriptor_layouts = scattering_density_lut_descriptor_set_layouts;
		scattering_density_lut_create_info.group_count_x = 256 / 4;
		scattering_density_lut_create_info.group_count_y = 128 / 4;
		scattering_density_lut_create_info.group_count_z = 32 / 4;
		scattering_density_lut_create_info.push_constants = scattering_order_push_constants;

		m_scattering_density_lut = _pipeline_manager->CreateComputePipeline(scattering_density_lut_create_info);

		// indirect irradiance

		ComputePipelineCreateInfo indirect_irradiance_lut_create_info;
		indirect_irradiance_lut_create_info.name = "indirect_irradiance_lut";
		indirect_irradiance_lut_create_info.cs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("atmosphere/indirect_irradiance_lut.comp.spv"));
		indirect_irradiance_lut_create_info.descriptor_layouts = indirect_irradiance_lut_descriptor_set_layouts;
		indirect_irradiance_lut_create_info.group_count_x = 64 / 8;
		indirect_irradiance_lut_create_info.group_count_y = 16 / 8;
		indirect_irradiance_lut_create_info.group_count_z = 1;
		indirect_irradiance_lut_create_info.push_constants = scattering_order_push_constants;
		m_indirect_irradiance_lut = _pipeline_manager->CreateComputePipeline(indirect_irradiance_lut_create_info);

		// multiple scattering lut 

		ComputePipelineCreateInfo multi_scattering_lut_create_info;
		multi_scattering_lut_create_info.name = "multi_scattering_lut";
		multi_scattering_lut_create_info.cs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("atmosphere/multi_scattering_lut.comp.spv"));
		multi_scattering_lut_create_info.descriptor_layouts = multi_scattering_lut_descriptor_set_layouts;
		multi_scattering_lut_create_info.group_count_x = 256 / 4;
		multi_scattering_lut_create_info.group_count_y = 128 / 4;
		multi_scattering_lut_create_info.group_count_z = 32 / 4;
		 
		m_multi_scattering_lut = _pipeline_manager->CreateComputePipeline(multi_scattering_lut_create_info);

		// camera volume

		//ComputePipelineCreateInfo camera_volume_create_info;
		//camera_volume_create_info.name = "camera_volume";
		//camera_volume_create_info.cs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("atmosphere/camera_volume.comp.spv"));
		//camera_volume_create_info.descriptor_layouts = camera_volume_descriptor_set_layouts;
		//camera_volume_create_info.group_count_x = 256 / 4;
		//camera_volume_create_info.group_count_y = 128 / 4;
		//camera_volume_create_info.group_count_z = 32 / 4;

		//m_camera_volume_pass = _pipeline_manager->CreateComputePipeline(camera_volume_create_info);

		// sky pass

		GraphicsPipelineCreateInfo sky_pipeline_create_info;
		sky_pipeline_create_info.name = "scatter";
		sky_pipeline_create_info.vs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("atmosphere/scatter.vert.spv"));
		sky_pipeline_create_info.ps = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("atmosphere/scatter.frag.spv"));
		sky_pipeline_create_info.descriptor_layouts = sky_descriptor_set_layout;

		std::vector<AttachmentCreateInfo> sky_attachments_create_info{
			{TextureFormat::TEXTURE_FORMAT_RGBA16_UNORM, COLOR_ATTACHMENT, TextureType::TEXTURE_TYPE_2D, _render_context.width, _render_context.height, 1}
		};

		m_sky_pass = _pipeline_manager->CreateGraphicsPipeline(sky_pipeline_create_info, sky_attachments_create_info, _render_context);


		m_sky_ub = std::make_shared<UniformBuffer>(_device);
		m_sky_ubdata.resolution = Math::vec2(_render_context.width, _render_context.height);

	}

	Atmosphere::~Atmosphere() noexcept
	{
	}

	void Atmosphere::SetCameraParams(Math::mat4 inv_view_projection, Math::vec3 camera_pos) noexcept
	{
		m_sky_ubdata.inv_view_projection_matrix = inv_view_projection;
		m_sky_ubdata.camera_pos = camera_pos;
		m_sky_ub->update(&m_sky_ubdata, sizeof(ScatteringUb));

	}

	void Atmosphere::UpdateDescriptorSets() noexcept
	{
		if (!precomputed)
		{
			// tramsmittance lut   
			m_transmittance_lut_descriptor_set_update_desc.BindResource(0, transmittance_lut);
			m_transmittance_lut_descriptor_set->UpdateDescriptorSet(m_transmittance_lut_descriptor_set_update_desc);


			// direct irradiance lut
			m_direct_irradiance_lut_descriptor_set_update_desc.BindResource(0, transmittance_lut);
			m_direct_irradiance_lut_descriptor_set_update_desc.BindResource(1, direct_irradiance_lut);
			m_direct_irradiance_lut_descriptor_set_update_desc.BindResource(2, _irradiance_tex);
			m_direct_irradiance_lut_descriptor_set->UpdateDescriptorSet(m_direct_irradiance_lut_descriptor_set_update_desc);

			// single scattering lut 

			//m_single_scattering_lut_ubdata.luminance_from_radiance = Math::mat3(1.0);
			//m_single_scattering_lut_ubdata.layer = 0;
			//m_single_scattering_lut_ub->update(&m_single_scattering_lut_ubdata,sizeof(m_single_scattering_lut_ubdata));

			m_single_scattering_lut_descriptor_set_update_desc.BindResource(0, transmittance_lut);
			//m_single_scattering_lut_descriptor_set_update_desc.BindResource(1, m_single_scattering_lut_ub);
			m_single_scattering_lut_descriptor_set_update_desc.BindResource(1, single_rayleigh_scattering_lut);
			m_single_scattering_lut_descriptor_set_update_desc.BindResource(2, single_mie_scattering_lut);
			m_single_scattering_lut_descriptor_set_update_desc.BindResource(3, _scattering_tex);

			m_single_scattering_lut_descriptor_set->UpdateDescriptorSet(m_single_scattering_lut_descriptor_set_update_desc);

			// SCATTERING DENSITY LUT

			m_scattering_density_lut_descriptor_set_update_desc.BindResource(0, transmittance_lut);
			m_scattering_density_lut_descriptor_set_update_desc.BindResource(1, single_rayleigh_scattering_lut);
			m_scattering_density_lut_descriptor_set_update_desc.BindResource(2, single_mie_scattering_lut);
			m_scattering_density_lut_descriptor_set_update_desc.BindResource(3, multi_scattering_lut);
			m_scattering_density_lut_descriptor_set_update_desc.BindResource(4, direct_irradiance_lut);
			m_scattering_density_lut_descriptor_set_update_desc.BindResource(5, scattering_density_lut);

			m_scattering_density_lut_descriptor_set->UpdateDescriptorSet(m_scattering_density_lut_descriptor_set_update_desc);

			// indirect irradiance

			m_indirect_irradiance_lut_descriptor_set_update_desc.BindResource(0, single_rayleigh_scattering_lut);
			m_indirect_irradiance_lut_descriptor_set_update_desc.BindResource(1, single_mie_scattering_lut);
			m_indirect_irradiance_lut_descriptor_set_update_desc.BindResource(2, multi_scattering_lut);
			m_indirect_irradiance_lut_descriptor_set_update_desc.BindResource(3, direct_irradiance_lut);
			m_indirect_irradiance_lut_descriptor_set_update_desc.BindResource(4, _irradiance_tex);

			m_indirect_irradiance_lut_descriptor_set->UpdateDescriptorSet(m_indirect_irradiance_lut_descriptor_set_update_desc);

			// multi-scattering

			m_multi_scattering_lut_descriptor_set_update_desc.BindResource(0, transmittance_lut);
			m_multi_scattering_lut_descriptor_set_update_desc.BindResource(1, scattering_density_lut);
			m_multi_scattering_lut_descriptor_set_update_desc.BindResource(2, single_rayleigh_scattering_lut);
			m_multi_scattering_lut_descriptor_set_update_desc.BindResource(3, _scattering_tex);

			m_multi_scattering_lut_descriptor_set->UpdateDescriptorSet(m_multi_scattering_lut_descriptor_set_update_desc);

		}
		// render sky

		m_sky_descriptor_set_update_desc.BindResource(0, m_sky_ub);
		m_sky_descriptor_set_update_desc.BindResource(1, transmittance_lut);
		m_sky_descriptor_set_update_desc.BindResource(2, _scattering_tex);
		m_sky_descriptor_set->UpdateDescriptorSet(m_sky_descriptor_set_update_desc);
	}

	void Atmosphere::BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) noexcept
	{
		m_sky_descriptor_set_update_desc.BindResource(binding, buffer);
	}

	std::shared_ptr<AttachmentDescriptor> Atmosphere::GetFrameBufferAttachment(u32 _index) const noexcept
	{

		return std::static_pointer_cast<GraphicsPipeline>(m_sky_pass)->GetFrameBufferAttachment(_index);
	}

	void Atmosphere::CreateResources(std::shared_ptr<Device> _device, std::shared_ptr<CommandBuffer> command_buffer) noexcept
	{

		// transmittance

		std::shared_ptr<DescriptorSetInfo> trasmittance_lut_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		trasmittance_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);

		m_transmittance_lut_descriptor_set = std::make_shared<DescriptorSet>(_device, trasmittance_lut_descriptor_set_create_info);

		transmittance_lut_descriptor_set_layouts = std::make_shared<DescriptorSetLayouts>();
		transmittance_lut_descriptor_set_layouts->layouts.push_back(m_transmittance_lut_descriptor_set->GetLayout());

		// direct irradiance

		std::shared_ptr<DescriptorSetInfo> direct_irradiance_lut_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		direct_irradiance_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER); // transmittance 
		direct_irradiance_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER); // delta_irradiance
		direct_irradiance_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER); // irradiance

		m_direct_irradiance_lut_descriptor_set = std::make_shared<DescriptorSet>(_device, direct_irradiance_lut_descriptor_set_create_info);

		direct_irradiance_lut_descriptor_set_layouts = std::make_shared<DescriptorSetLayouts>();
		direct_irradiance_lut_descriptor_set_layouts->layouts.push_back(m_direct_irradiance_lut_descriptor_set->GetLayout());

		// single scattering lut pass
		m_single_scattering_lut_ub = std::make_shared<UniformBuffer>(_device);

		std::shared_ptr<DescriptorSetInfo> single_scattering_lut_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		single_scattering_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		//single_scattering_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_COMPUTE_SHADER);
		single_scattering_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		single_scattering_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		single_scattering_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);

		m_single_scattering_lut_descriptor_set = std::make_shared<DescriptorSet>(_device, single_scattering_lut_descriptor_set_create_info);

		single_scattering_lut_descriptor_set_layouts = std::make_shared<DescriptorSetLayouts>();
		single_scattering_lut_descriptor_set_layouts->layouts.push_back(m_single_scattering_lut_descriptor_set->GetLayout());

		// scattering density


		std::shared_ptr<DescriptorSetInfo> scattering_density_lut_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		scattering_density_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		scattering_density_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		scattering_density_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		scattering_density_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		scattering_density_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		scattering_density_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);

		m_scattering_density_lut_descriptor_set = std::make_shared<DescriptorSet>(_device, scattering_density_lut_descriptor_set_create_info);

		scattering_density_lut_descriptor_set_layouts = std::make_shared<DescriptorSetLayouts>();
		scattering_density_lut_descriptor_set_layouts->layouts.push_back(m_scattering_density_lut_descriptor_set->GetLayout());

		// INDIRECT IRRADIANCE

		std::shared_ptr<DescriptorSetInfo> indirect_irradiance_lut_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		indirect_irradiance_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		indirect_irradiance_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		indirect_irradiance_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		indirect_irradiance_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		indirect_irradiance_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);

		m_indirect_irradiance_lut_descriptor_set = std::make_shared<DescriptorSet>(_device, indirect_irradiance_lut_descriptor_set_create_info);

		indirect_irradiance_lut_descriptor_set_layouts = std::make_shared<DescriptorSetLayouts>();
		indirect_irradiance_lut_descriptor_set_layouts->layouts.push_back(m_indirect_irradiance_lut_descriptor_set->GetLayout());

		// multi scattering

		std::shared_ptr<DescriptorSetInfo> multi_scatteing_lut_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		multi_scatteing_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		multi_scatteing_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		multi_scatteing_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);
		multi_scatteing_lut_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);

		m_multi_scattering_lut_descriptor_set = std::make_shared<DescriptorSet>(_device, multi_scatteing_lut_descriptor_set_create_info);

		multi_scattering_lut_descriptor_set_layouts = std::make_shared<DescriptorSetLayouts>();
		multi_scattering_lut_descriptor_set_layouts->layouts.push_back(m_multi_scattering_lut_descriptor_set->GetLayout());

		//// camera volume

		//std::shared_ptr<DescriptorSetInfo> camera_volume_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		//camera_volume_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_COMPUTE_SHADER); // camera pos, inv vp, resolution
		//camera_volume_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER); // transmittion
		//camera_volume_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_COMPUTE_SHADER); // scattering
		//camera_volume_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER); // scatter
		//camera_volume_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER); // t

		//m_camera_volume_descriptor_set = std::make_shared<DescriptorSet>(_device, camera_volume_descriptor_set_create_info);

		//camera_volume_descriptor_set_layouts = std::make_shared<DescriptorSetLayouts>();
		//camera_volume_descriptor_set_layouts->layouts.push_back(m_camera_volume_descriptor_set->GetLayout());

		// sky pass

		std::shared_ptr<DescriptorSetInfo> scatter_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		scatter_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_PIXEL_SHADER); // camera pos, inv vp, resolution
		scatter_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_PIXEL_SHADER); // transmittion
		scatter_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_PIXEL_SHADER); // scattering
		scatter_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_PIXEL_SHADER); // geometry
		scatter_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_PIXEL_SHADER); // depth

		m_sky_descriptor_set = std::make_shared<DescriptorSet>(_device, scatter_descriptor_set_create_info);

		sky_descriptor_set_layout = std::make_shared<DescriptorSetLayouts>();
		sky_descriptor_set_layout->layouts.push_back(m_sky_descriptor_set->GetLayout());


		// textures and uniform buffers

		transmittance_lut = std::make_shared<Texture>(_device, command_buffer, TextureCreateInfo{ TextureType::TEXTURE_TYPE_2D,TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT,TextureUsage::TEXTURE_USAGE_RW, 256, 64, 1 });
		direct_irradiance_lut = std::make_shared<Texture>(_device, command_buffer, TextureCreateInfo{ TextureType::TEXTURE_TYPE_2D,TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT,TextureUsage::TEXTURE_USAGE_RW, 64, 16, 1 });
		_irradiance_tex = std::make_shared<Texture>(_device, command_buffer, TextureCreateInfo{ TextureType::TEXTURE_TYPE_2D,TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT,TextureUsage::TEXTURE_USAGE_RW, 64, 16, 1 });
		single_rayleigh_scattering_lut = std::make_shared<Texture>(_device, command_buffer, TextureCreateInfo{ TextureType::TEXTURE_TYPE_3D,TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT,TextureUsage::TEXTURE_USAGE_RW, 256, 128, 32 });
		single_mie_scattering_lut = std::make_shared<Texture>(_device, command_buffer, TextureCreateInfo{ TextureType::TEXTURE_TYPE_3D,TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT,TextureUsage::TEXTURE_USAGE_RW, 256, 128, 32 });
		_scattering_tex = std::make_shared<Texture>(_device, command_buffer, TextureCreateInfo{ TextureType::TEXTURE_TYPE_3D,TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT,TextureUsage::TEXTURE_USAGE_RW, 256, 128, 32 });
		scattering_density_lut = std::make_shared<Texture>(_device, command_buffer, TextureCreateInfo{ TextureType::TEXTURE_TYPE_3D,TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT,TextureUsage::TEXTURE_USAGE_RW, 256, 128, 32 });
		multi_scattering_lut = single_rayleigh_scattering_lut;

		//scatter_transfer_t = std::make_shared<Texture>(_device, command_buffer, TextureCreateInfo{ TextureType::TEXTURE_TYPE_3D,TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT,TextureUsage::TEXTURE_USAGE_RW, 32, 32, 32 });
		//out_transmittance = std::make_shared<Texture>(_device, command_buffer, TextureCreateInfo{ TextureType::TEXTURE_TYPE_3D,TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT,TextureUsage::TEXTURE_USAGE_RW, 32, 32, 32 });

		scattering_order_push_constants = std::make_shared<PushConstants>();
		scattering_order_push_constants->ranges = { {SHADER_STAGE_COMPUTE_SHADER, 0, 2 * sizeof(Math::mat4)} }; // Push constants have a minimum size of 128 bytes


	}

}