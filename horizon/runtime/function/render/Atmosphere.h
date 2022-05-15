#pragma once

#include <runtime/function/render/rhi/Descriptors.h>
#include <runtime/function/render/rhi/Pipeline.h>
#include <runtime/function/render/rhi/UniformBuffer.h>
#include <runtime/function/render/rhi/Texture.h>
#include <runtime/function/render/rhi/CommandBuffer.h>
#include <memory>

namespace Horizon
{
	class Atmosphere
	{
	public:
		Atmosphere(
			std::shared_ptr<PipelineManager> _pipeline_manager,
			std::shared_ptr<Device> _device,
			std::shared_ptr<CommandBuffer> command_buffer,
			RenderContext& _render_context) noexcept;
		~Atmosphere() noexcept;
		void SetCameraParams(Math::mat4 inv_view_projection, Math::vec3 camera_pos) noexcept;
		void UpdateDescriptorSets() noexcept;
		void BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) noexcept;
		std::shared_ptr<AttachmentDescriptor> GetFrameBufferAttachment(u32 _index) const noexcept;

	private:
		void CreateResources(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer) noexcept;
	public:
		std::shared_ptr<Pipeline> m_sky_pass, m_transmittance_lut_pass,
			m_direct_irradiance_lut_pass,
			m_single_scattering_lut_pass,
			m_scattering_density_lut,
			m_indirect_irradiance_lut,
			m_multi_scattering_lut,
			m_camera_volume_pass;
		std::shared_ptr<DescriptorSet> m_sky_descriptor_set,
			m_transmittance_lut_descriptor_set,
			m_direct_irradiance_lut_descriptor_set,
			m_single_scattering_lut_descriptor_set,
			m_scattering_density_lut_descriptor_set,
			m_indirect_irradiance_lut_descriptor_set,
			m_multi_scattering_lut_descriptor_set,
			m_camera_volume_descriptor_set;
		u32 m_multi_scattering_order = 3;

		std::shared_ptr<PushConstants> scattering_order_push_constants;

		std::array<i32, 4> layers = {1, 2, 3, 4 };
	private:
		std::shared_ptr<DescriptorSetLayouts> transmittance_lut_descriptor_set_layouts;
		std::shared_ptr<DescriptorSetLayouts> direct_irradiance_lut_descriptor_set_layouts;
		std::shared_ptr<DescriptorSetLayouts> single_scattering_lut_descriptor_set_layouts;
		std::shared_ptr<DescriptorSetLayouts> scattering_density_lut_descriptor_set_layouts;
		std::shared_ptr<DescriptorSetLayouts> indirect_irradiance_lut_descriptor_set_layouts;
		std::shared_ptr<DescriptorSetLayouts> multi_scattering_lut_descriptor_set_layouts;
		std::shared_ptr<DescriptorSetLayouts> camera_volume_descriptor_set_layouts;
		std::shared_ptr<DescriptorSetLayouts> sky_descriptor_set_layout;

		DescriptorSetUpdateDesc m_sky_descriptor_set_update_desc;
		DescriptorSetUpdateDesc m_transmittance_lut_descriptor_set_update_desc;
		DescriptorSetUpdateDesc m_direct_irradiance_lut_descriptor_set_update_desc;
		DescriptorSetUpdateDesc m_single_scattering_lut_descriptor_set_update_desc;
		DescriptorSetUpdateDesc m_scattering_density_lut_descriptor_set_update_desc;
		DescriptorSetUpdateDesc m_indirect_irradiance_lut_descriptor_set_update_desc;
		DescriptorSetUpdateDesc m_multi_scattering_lut_descriptor_set_update_desc;
		DescriptorSetUpdateDesc m_camera_volume_descriptor_set_update_desc;

	public:
		std::shared_ptr<Texture> transmittance_lut;
		std::shared_ptr<Texture> direct_irradiance_lut;
		std::shared_ptr<Texture> _irradiance_tex;
		std::shared_ptr<Texture> single_rayleigh_scattering_lut;
		std::shared_ptr<Texture> single_mie_scattering_lut;
		std::shared_ptr<Texture> _scattering_tex;
		//std::shared_ptr<Texture> single_scattering_lut_tex4;
		std::shared_ptr<Texture> scattering_density_lut;
		std::shared_ptr<Texture> multi_scattering_lut;

		std::shared_ptr<Texture> scatter_transfer_t;
		std::shared_ptr<Texture> out_transmittance;

	private:

		std::shared_ptr<UniformBuffer> m_single_scattering_lut_ub;
		struct SingleScatteringLUTUb
		{
			Math::mat3 luminance_from_radiance;
			i32 layer;
		} m_single_scattering_lut_ubdata;

		std::shared_ptr<UniformBuffer> m_sky_ub;
	public:
		struct ScatteringUb
		{
			Math::mat4 inv_view_projection_matrix;
			Math::vec2 resolution;
			Math::vec2 pad0;
			Math::vec3 camera_pos;
			f32 pad1;
		} m_sky_ubdata;

		bool precomputed = false;


	};

}
