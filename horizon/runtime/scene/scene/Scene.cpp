#include "Scene.h"

#include <runtime/core/log/Log.h>
#include <runtime/function/render/rhi/UniformBuffer.h>

namespace Horizon {

	Scene::Scene(RenderContext& render_context, std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer) :m_render_context(render_context), m_device(device), m_command_buffer(command_buffer)
	{


		std::shared_ptr<DescriptorSetInfo> sceneDescriptorSetInfo = std::make_shared<DescriptorSetInfo>();
		// vp mat
		sceneDescriptorSetInfo->AddBinding(DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_VERTEX_SHADER | SHADER_STAGE_PIXEL_SHADER);
		//// light count
		//sceneDescriptorSetInfo->AddBinding(DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_PIXEL_SHADER);
		//// light ub
		//sceneDescriptorSetInfo->AddBinding(DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_PIXEL_SHADER);
		//// camera
		//sceneDescriptorSetInfo->AddBinding(DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_PIXEL_SHADER);

		m_scene_descriptor_set = std::make_shared<DescriptorSet>(m_device, sceneDescriptorSetInfo);

		m_camera = std::make_shared<Camera>(Math::vec3(0.0f, 0.0f, 10000.0f), Math::vec3(0.0f, 0.0f, 0.0f), Math::vec3(0.0f, 1.0f, 0.0f));
		m_camera->SetPerspectiveProjectionMatrix(Math::radians(90.0f), static_cast<f32>(m_render_context.width) / static_cast<f32>(m_render_context.height), 100.0f, 20000.0f);
		m_camera->SetCameraSpeed(10.0f);

		// create uniform buffer
		m_scene_ub = std::make_shared<UniformBuffer>(device);
		//m_light_count_ub = std::make_shared<UniformBuffer>(device);
		//m_light_ub = std::make_shared<UniformBuffer>(device);
		m_camera_ub = std::make_shared<UniformBuffer>(device);
	}

	Scene::~Scene()
	{

	}

	void Scene::LoadModel(const std::string& path, const std::string& name)
	{
		m_models.insert({ name, std::make_shared<Model>(path, m_device, m_command_buffer, m_scene_descriptor_set) });
	}

	std::shared_ptr<Model> Scene::GetModel(const std::string& name)
	{
		return m_models.at(name);
	}

	void Scene::AddDirectLight(Math::vec3 color, f32 intensity, Math::vec3 direction)
	{
		if (m_light_count_ubdata.lightCount >= MAX_LIGHT_COUNT) {
			LOG_WARN("light count cannot more than {}", MAX_LIGHT_COUNT);
			return;
		}

		f32 luminous_intensity = intensity;

		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].color_intensity = { color, luminous_intensity };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].direction = { direction.x, direction.y, direction.z, 0.0 };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].position_type = { 0.0, 0.0, 0.0, static_cast<f32>(LightType::DIRECT_LIGHT) };
		m_light_count_ubdata.lightCount++;

	}

	void Scene::AddPointLight(Math::vec3 color, f32 intensity, Math::vec3 position, f32 radius)
	{
		if (m_light_count_ubdata.lightCount >= MAX_LIGHT_COUNT) {
			LOG_WARN("light count cannot more than {}", MAX_LIGHT_COUNT);
			return;
		}

		f32 luminous_intensity = intensity / Math::one_over_pi<f32>() / 4.0f;

		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].color_intensity = { color, luminous_intensity };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].position_type = { position, static_cast<f32>(LightType::POINT_LIGHT) };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].radius_inner_outer = { radius, 0.0, 0.0, 0.0 };
		m_light_count_ubdata.lightCount++;

	}

	void Scene::AddSpotLight(Math::vec3 color, f32 intensity, Math::vec3 direction, Math::vec3 position, f32 radius, f32 innerConeAngle, f32 outerConeAngle)
	{
		if (m_light_count_ubdata.lightCount >= MAX_LIGHT_COUNT) {
			LOG_WARN("light count cannot more than {}", MAX_LIGHT_COUNT);
			return;
		}
		;
		f32 cos_outer = Math::cos(std::clamp(std::abs(outerConeAngle), 0.5f * Math::radians(0.5f), Math::two_pi<f32>()));
		f32 cos_outer2 = Math::sqrt(cos_outer * cos_outer);
		f32 luminous_intensity = intensity / Math::one_over_two_pi<f32>() / (1.0f - cos_outer2);

		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].color_intensity = { color, luminous_intensity };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].direction = { direction, 0.0 };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].position_type = { position, static_cast<f32>(LightType::SPOT_LIGHT) };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].radius_inner_outer = { radius, innerConeAngle, outerConeAngle, 0.0 };
		m_light_count_ubdata.lightCount++;
	}

	void Scene::Prepare()
	{
		// update scene descriptorset
		m_scene_descriptor_set->AllocateDescriptorSet();

		// update Ub data
		m_scene_ubdata.view = m_camera->GetViewMatrix();
		m_scene_ubdata.projection = m_camera->GetProjectionMatrix();
		m_scene_ubdata.nearFar = m_camera->GetNearFarPlane();
		m_scene_ub->update(&m_scene_ubdata, sizeof(SceneUb));

		m_camera_ubdata.camera_pos = m_camera->GetPosition();
		m_camera_ubdata.camera_forward_dir = m_camera->GetForwardDir();
		m_camera_ub->update(&m_camera_ubdata, sizeof(CamaeraUb));

		//m_light_count_ub->update(&m_light_count_ubdata, sizeof(LightCountUb));
		//m_light_ub->update(&m_lights_ubdata, m_light_count_ubdata.lightCount > 0 ? sizeof(LightParams) * m_light_count_ubdata.lightCount : sizeof(LightParams));


		DescriptorSetUpdateDesc desc;
		desc.BindResource(0, m_scene_ub);
		//desc.BindResource(1, m_light_count_ub);
		//desc.BindResource(2, m_light_ub);
		//desc.BindResource(3, m_camera_ub);

		m_scene_descriptor_set->UpdateDescriptorSet(desc);
		


		// update material&mesh descriptorset
		for (auto& model : m_models) {
			model.second->UpdateModelMatrix();
			model.second->UpdateDescriptors();
		}
	}

	void Scene::Draw(u32 _i, std::shared_ptr<CommandBuffer> _command_buffer, std::shared_ptr<Pipeline> _pipeline) {

		_command_buffer->beginRenderPass(_i, _pipeline);
		for (auto& model : m_models) {
			model.second->Draw(_pipeline, _command_buffer->Get(_i));
		}
		_command_buffer->endRenderPass(_i);
	}


	std::shared_ptr<DescriptorSetLayouts> Scene::GetDescriptorLayouts()
	{
		std::shared_ptr<DescriptorSetLayouts> layouts = std::make_shared<DescriptorSetLayouts>();
		VkDescriptorSetLayout materialSetLayout;
		for (auto& model : m_models) {
			if (model.second->GetMaterialDescriptorSet()) {
				//meshSetLayout = model.second->getMeshDescriptorSet()->GetLayout();
				materialSetLayout = model.second->GetMaterialDescriptorSet()->GetLayout();
			}
		}
		//if (!meshSetLayout) {
		//	LOG_ERROR("mesh descriptorset layout not found");
		//}
		if (!materialSetLayout) {
			LOG_ERROR("material descriptorset layout not found");
		}
		layouts->layouts = { { m_scene_descriptor_set->GetLayout(), materialSetLayout} };
		return layouts;
	}

	std::shared_ptr<DescriptorSetLayouts> Scene::GetGeometryPassDescriptorLayouts()
	{
		std::shared_ptr<DescriptorSetLayouts> layouts = std::make_shared<DescriptorSetLayouts>();
		VkDescriptorSetLayout materialSetLayout;
		for (auto& model : m_models) {
			if (model.second->GetMaterialDescriptorSet()) {
				materialSetLayout = model.second->GetMaterialDescriptorSet()->GetLayout();
			}
		}
		if (!materialSetLayout) {
			LOG_ERROR("material descriptorset layout not found");
		}
		layouts->layouts = { { m_scene_descriptor_set->GetLayout(), materialSetLayout} };
		return layouts;
	}

	std::shared_ptr<DescriptorSetLayouts> Scene::GetSceneDescriptorLayouts()
	{
		std::shared_ptr<DescriptorSetLayouts> layouts = std::make_shared<DescriptorSetLayouts>();
		layouts->layouts.emplace_back(m_scene_descriptor_set->GetLayout());
		return layouts;
	}


	std::shared_ptr<Camera> Scene::GetMainCamera() const
	{
		return m_camera;
	}
	std::shared_ptr<UniformBuffer> Scene::getCameraUbo() const
	{
		return m_camera_ub;
	}
	FullscreenTriangle::FullscreenTriangle(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer) :m_device(device), m_command_buffer(command_buffer)
	{

		// pos, normal, uv
		m_vertices.push_back(Vertex{ { 1.0f, -1.0f, 0.0f}, {0.0f,0.0f,0.0f}, {1.0f, 0.0f} });
		m_vertices.push_back(Vertex{ { 1.0f,  3.0f, 0.0f}, {0.0f,0.0f,0.0f}, {1.0f, -2.0f} });
		m_vertices.push_back(Vertex{ {-3.0f, -1.0f, 0.0f}, {0.0f,0.0f,0.0f}, {-1.0f, 0.0f} });
		m_vertex_buffer = std::make_shared<VertexBuffer>(m_device, m_command_buffer, m_vertices);
	}

	void FullscreenTriangle::Draw(u32 _i, std::shared_ptr<CommandBuffer> _command_buffer, std::shared_ptr<Pipeline> _pipeline, const std::vector<std::shared_ptr<DescriptorSet>> _descriptor_sets, bool _is_present)
	{
		_command_buffer->beginRenderPass(_i, _pipeline, _is_present);
		VkCommandBuffer command_buffer = _command_buffer->Get(_i);
		const VkDeviceSize offsets[1] = { 0 };
		VkBuffer vertexBuffer = m_vertex_buffer->Get();
		vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertexBuffer, offsets);

		std::vector<VkDescriptorSet> descriptor_sets(_descriptor_sets.size());
		for (u32 i = 0; i < _descriptor_sets.size(); i++)
		{
			descriptor_sets[i] = _descriptor_sets[i]->Get();
		}

		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->GetLayout(), 0, descriptor_sets.size(), descriptor_sets.data(), 0, 0);
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->Get());
		vkCmdDraw(command_buffer, 3, 1, 0, 0);
		_command_buffer->endRenderPass(_i);
	}


}