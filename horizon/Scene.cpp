#include "Scene.h"
#include "UniformBuffer.h"

namespace Horizon {

	Scene::Scene(RenderContext& render_context, std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer) :m_render_context(render_context), m_device(device), m_command_buffer(command_buffer)
	{


		std::shared_ptr<DescriptorSetInfo> sceneDescriptorSetInfo = std::make_shared<DescriptorSetInfo>();
		// vp mat
		sceneDescriptorSetInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		//// light count
		//sceneDescriptorSetInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		//// light ub
		//sceneDescriptorSetInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		//// camera
		//sceneDescriptorSetInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

		m_scene_descriptor_set = std::make_shared<DescriptorSet>(m_device, sceneDescriptorSetInfo);

		m_camera = std::make_shared<Camera>(Math::vec3(0.0f, 0.0f, 10000.0f), Math::vec3(0.0f, 0.0f, 0.0f), Math::vec3(0.0f, 1.0f, 0.0f));
		m_camera->setPerspectiveProjectionMatrix(Math::radians(90.0f), static_cast<f32>(m_render_context.width) / static_cast<f32>(m_render_context.height), 0.01f, 100000.0f);
		m_camera->setCameraSpeed(10.0f);

		// create uniform buffer
		m_scene_ub = std::make_shared<UniformBuffer>(device);
		//m_light_count_ub = std::make_shared<UniformBuffer>(device);
		//m_light_ub = std::make_shared<UniformBuffer>(device);
		m_camera_ub = std::make_shared<UniformBuffer>(device);
	}

	Scene::~Scene()
	{

	}

	void Scene::loadModel(const std::string& path, const std::string& name)
	{
		m_models.insert({ name, std::make_shared<Model>(path, m_device, m_command_buffer, m_scene_descriptor_set) });
	}

	std::shared_ptr<Model> Scene::getModel(const std::string& name)
	{
		return m_models.at(name);
	}

	void Scene::addDirectLight(Math::vec3 color, f32 intensity, Math::vec3 direction)
	{
		if (m_light_count_ubdata.lightCount >= MAX_LIGHT_COUNT) {
			spdlog::warn("light count cannot more than {}", MAX_LIGHT_COUNT);
			return;
		}
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].color_intensity = { color, intensity };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].direction = { direction.x, direction.y, direction.z, 0.0 };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].position_type = { 0.0, 0.0, 0.0, static_cast<f32>(LightType::DIRECT_LIGHT) };
		m_light_count_ubdata.lightCount++;

	}

	void Scene::addPointLight(Math::vec3 color, f32 intensity, Math::vec3 position, f32 radius)
	{
		if (m_light_count_ubdata.lightCount >= MAX_LIGHT_COUNT) {
			spdlog::warn("light count cannot more than {}", MAX_LIGHT_COUNT);
			return;
		}

		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].color_intensity = { color, intensity };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].position_type = { position, static_cast<f32>(LightType::POINT_LIGHT) };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].radius_inner_outer = { radius, 0.0, 0.0, 0.0 };
		m_light_count_ubdata.lightCount++;

	}

	void Scene::addSpotLight(Math::vec3 color, f32 intensity, Math::vec3 direction, Math::vec3 position, f32 radius, f32 innerConeAngle, f32 outerConeAngle)
	{
		if (m_light_count_ubdata.lightCount >= MAX_LIGHT_COUNT) {
			spdlog::warn("light count cannot more than {}", MAX_LIGHT_COUNT);
			return;
		}
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].color_intensity = { color, intensity };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].direction = { direction, 0.0 };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].position_type = { position, static_cast<f32>(LightType::SPOT_LIGHT) };
		m_lights_ubdata.lights[m_light_count_ubdata.lightCount].radius_inner_outer = { radius, innerConeAngle, outerConeAngle, 0.0 };
		m_light_count_ubdata.lightCount++;
	}

	void Scene::prepare()
	{
		// update scene descriptorset
		m_scene_descriptor_set->allocateDescriptorSet();

		// update Ub data
		m_scene_ubdata.view = m_camera->getViewMatrix();
		m_scene_ubdata.projection = m_camera->getProjectionMatrix();
		m_scene_ubdata.nearFar = m_camera->getNearFarPlane();
		m_scene_ub->update(&m_scene_ubdata, sizeof(SceneUb));

		m_camera_ubdata.cameraPos = m_camera->getPosition();
		m_camera_ub->update(&m_camera_ubdata, sizeof(CamaeraUb));

		//m_light_count_ub->update(&m_light_count_ubdata, sizeof(LightCountUb));
		//m_light_ub->update(&m_lights_ubdata, m_light_count_ubdata.lightCount > 0 ? sizeof(LightParams) * m_light_count_ubdata.lightCount : sizeof(LightParams));


		DescriptorSetUpdateDesc desc;
		desc.bindResource(0, m_scene_ub);
		//desc.bindResource(1, m_light_count_ub);
		//desc.bindResource(2, m_light_ub);
		//desc.bindResource(3, m_camera_ub);

		m_scene_descriptor_set->UpdateDescriptorSet(desc);
		


		// update material&mesh descriptorset
		for (auto& model : m_models) {
			model.second->updateModelMatrix();
			model.second->updateDescriptors();
		}
	}

	void Scene::draw(VkCommandBuffer command_buffer, std::shared_ptr<Pipeline> pipeline) {
		for (auto& model : m_models) {
			model.second->draw(pipeline, command_buffer);
		}
	}


	std::shared_ptr<DescriptorSetLayouts> Scene::getDescriptorLayouts()
	{
		std::shared_ptr<DescriptorSetLayouts> layouts = std::make_shared<DescriptorSetLayouts>();
		VkDescriptorSetLayout materialSetLayout;
		for (auto& model : m_models) {
			if (model.second->getMaterialDescriptorSet()) {
				//meshSetLayout = model.second->getMeshDescriptorSet()->getLayout();
				materialSetLayout = model.second->getMaterialDescriptorSet()->getLayout();
			}
		}
		//if (!meshSetLayout) {
		//	spdlog::error("mesh descriptorset layout not found");
		//}
		if (!materialSetLayout) {
			spdlog::error("material descriptorset layout not found");
		}
		layouts->layouts = { { m_scene_descriptor_set->getLayout(), materialSetLayout} };
		return layouts;
	}

	std::shared_ptr<DescriptorSetLayouts> Scene::getGeometryPassDescriptorLayouts()
	{
		std::shared_ptr<DescriptorSetLayouts> layouts = std::make_shared<DescriptorSetLayouts>();
		VkDescriptorSetLayout materialSetLayout;
		for (auto& model : m_models) {
			if (model.second->getMaterialDescriptorSet()) {
				materialSetLayout = model.second->getMaterialDescriptorSet()->getLayout();
			}
		}
		if (!materialSetLayout) {
			spdlog::error("material descriptorset layout not found");
		}
		layouts->layouts = { { m_scene_descriptor_set->getLayout(), materialSetLayout} };
		return layouts;
	}

	std::shared_ptr<DescriptorSetLayouts> Scene::getSceneDescriptorLayouts()
	{
		std::shared_ptr<DescriptorSetLayouts> layouts = std::make_shared<DescriptorSetLayouts>();
		layouts->layouts.emplace_back(m_scene_descriptor_set->getLayout());
		return layouts;
	}


	std::shared_ptr<Camera> Scene::getMainCamera() const
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

	void FullscreenTriangle::draw(VkCommandBuffer command_buffer, std::shared_ptr<Pipeline> pipeline, const std::vector<std::shared_ptr<DescriptorSet>> _descriptorsets, bool is_present)
	{

		const VkDeviceSize offsets[1] = { 0 };
		VkBuffer vertexBuffer = m_vertex_buffer->get();
		vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertexBuffer, offsets);

		std::vector<VkDescriptorSet> descriptorsets(_descriptorsets.size());
		for (u32 i = 0; i < _descriptorsets.size(); i++)
		{
			descriptorsets[i] = _descriptorsets[i]->get();
		}

		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getLayout(), 0, descriptorsets.size(), descriptorsets.data(), 0, 0);
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->get());
		vkCmdDraw(command_buffer, 3, 1, 0, 0);

	}


}