#include "Scene.h"
#include "UniformBuffer.h"

namespace Horizon {

	Scene::Scene(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> commandBuffer, u32 w, u32 h) :mDevice(device), mCommandBuffer(commandBuffer), mWidth(w), mHeight(h)
	{


		std::shared_ptr<DescriptorSetInfo> sceneDescriptorSetInfo = std::make_shared<DescriptorSetInfo>();
		// vp mat
		sceneDescriptorSetInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		// light count
		sceneDescriptorSetInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		// light ub
		sceneDescriptorSetInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		// camera
		sceneDescriptorSetInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

		sceneDescritporSet = std::make_shared<DescriptorSet>(mDevice, sceneDescriptorSetInfo);

		mCamera = std::make_shared<Camera>(Math::vec3(0.0f, 0.0f, 5.0f), Math::vec3(0.0f, 0.0f, 0.0f), Math::vec3(0.0f, 1.0f, 0.0f));
		mCamera->setPerspectiveProjectionMatrix(Math::radians(90.0f), static_cast<f32>(mWidth) / static_cast<f32>(mHeight), 0.01f, 1000.0f);
		mCamera->setCameraSpeed(0.01f);

		// create uniform buffer
		sceneUb = std::make_shared<UniformBuffer>(device);
		lightCountUb = std::make_shared<UniformBuffer>(device);
		lightUb = std::make_shared<UniformBuffer>(device);
		cameraUb = std::make_shared<UniformBuffer>(device);
	}

	Scene::~Scene()
	{

	}

	void Scene::loadModel(const std::string& path)
	{
		mModels.emplace_back(std::make_shared<Model>(path, mDevice, mCommandBuffer, sceneDescritporSet));
	}

	void Scene::addDirectLight(Math::vec3 color, f32 intensity, Math::vec3 direction)
	{
		if (lightCountUbStruct.lightCount >= MAX_LIGHT_COUNT) {
			spdlog::warn("light count cannot more than {}", MAX_LIGHT_COUNT);
			return;
		}
		lightUbStruct.lights[lightCountUbStruct.lightCount].colorIntensity = { color, intensity };
		lightUbStruct.lights[lightCountUbStruct.lightCount].direction = { direction.x, direction.y, direction.z, 0.0 };
		lightUbStruct.lights[lightCountUbStruct.lightCount].positionType = { 0.0, 0.0, 0.0, static_cast<f32>(LightType::DIRECT_LIGHT) };
		lightCountUbStruct.lightCount++;

	}

	void Scene::addPointLight(Math::vec3 color, f32 intensity, Math::vec3 position, f32 radius)
	{
		if (lightCountUbStruct.lightCount >= MAX_LIGHT_COUNT) {
			spdlog::warn("light count cannot more than {}", MAX_LIGHT_COUNT);
			return;
		}

		lightUbStruct.lights[lightCountUbStruct.lightCount].colorIntensity = { color, intensity };
		lightUbStruct.lights[lightCountUbStruct.lightCount].positionType = { position, static_cast<f32>(LightType::POINT_LIGHT) };
		lightUbStruct.lights[lightCountUbStruct.lightCount].radiusInnerOuter = { radius, 0.0, 0.0, 0.0 };
		lightCountUbStruct.lightCount++;

	}

	void Scene::addSpotLight(Math::vec3 color, f32 intensity, Math::vec3 direction, Math::vec3 position, f32 radius, f32 innerConeAngle, f32 outerConeAngle)
	{
		if (lightCountUbStruct.lightCount >= MAX_LIGHT_COUNT) {
			spdlog::warn("light count cannot more than {}", MAX_LIGHT_COUNT);
			return;
		}
		lightUbStruct.lights[lightCountUbStruct.lightCount].colorIntensity = { color, intensity };
		lightUbStruct.lights[lightCountUbStruct.lightCount].direction = { direction, 0.0 };
		lightUbStruct.lights[lightCountUbStruct.lightCount].positionType = { position, static_cast<f32>(LightType::SPOT_LIGHT) };
		lightUbStruct.lights[lightCountUbStruct.lightCount].radiusInnerOuter = {radius, innerConeAngle, outerConeAngle, 0.0 };
		lightCountUbStruct.lightCount++;
	}

	void Scene::prepare()
	{
		sceneDescritporSet->allocateDescriptorSet();

		// update Ub data
		sceneUbStruct.view = mCamera->getViewMatrix();
		sceneUbStruct.projection = mCamera->getProjectionMatrix();
		camaeraUbStruct.cameraPos = mCamera->getPosition();

		sceneUb->update(&sceneUbStruct, sizeof(SceneUbStruct));
		lightCountUb->update(&lightCountUbStruct, sizeof(LightCountUbStruct));
		lightUb->update(&lightUbStruct, lightCountUbStruct.lightCount > 0 ? sizeof(LightParams) * lightCountUbStruct.lightCount : sizeof(LightParams));
		cameraUb->update(&camaeraUbStruct, sizeof(CamaeraUbStruct));

		DescriptorSetUpdateDesc desc;
		desc.addBinding(0, sceneUb);
		desc.addBinding(1, lightCountUb);
		desc.addBinding(2, lightUb);
		desc.addBinding(3, cameraUb);

		sceneDescritporSet->updateDescriptorSet(desc);
		for (auto& model : mModels) {
			model->updateDescriptors();
		}
	}
	
	void Scene::draw(std::shared_ptr<Pipeline> pipeline) {
		for (u32 i = 0; i < mCommandBuffer->commandBufferCount(); i++) {
			mCommandBuffer->beginRenderPass(i, pipeline);

			auto commandBuffer = *mCommandBuffer->get(i);
			for (auto& model : mModels) {
				model->draw(pipeline, commandBuffer);
			}
			mCommandBuffer->endRenderPass(i);
		}
	}


	std::shared_ptr<DescriptorSetLayouts> Scene::getDescriptorLayouts()
	{
		std::shared_ptr<DescriptorSetLayouts> layouts = std::make_shared<DescriptorSetLayouts>();
		VkDescriptorSetLayout meshSetLayout, materialSetLayout;
		for (auto& model : mModels) {
			if (model->getMaterialDescriptorSet() && model->getMeshDescriptorSet()) {
				meshSetLayout = model->getMeshDescriptorSet()->getLayout();
				materialSetLayout = model->getMaterialDescriptorSet()->getLayout();
			}
		}
		if (!meshSetLayout) {
			spdlog::error("mesh descriptorset layout not found");
		}
		if (!materialSetLayout) {
			spdlog::error("material descriptorset layout not found");
		}
		layouts->layouts = { { sceneDescritporSet->getLayout(), materialSetLayout, meshSetLayout} };
		return layouts;
	}

	std::shared_ptr<Camera> Scene::getMainCamera() const
	{
		return mCamera;
	}
}