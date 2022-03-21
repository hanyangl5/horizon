#include "Scene.h"
#include "UniformBuffer.h"

namespace Horizon {

	Scene::Scene(Device* device, CommandBuffer* commandBuffer, u32 w, u32 h) :mDevice(device), mCommandBuffer(commandBuffer), mWidth(w), mHeight(h)
	{

		DescriptorSetInfo sceneDescriptorSetInfo{};
		// vp mat
		sceneDescriptorSetInfo.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		// light count
		sceneDescriptorSetInfo.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		// light ub
		sceneDescriptorSetInfo.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		// camera
		sceneDescriptorSetInfo.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

		sceneDescritporSet = new DescriptorSet(mDevice, &sceneDescriptorSetInfo);

		mCamera = new Camera(vec3(0.0f, 0.0f, 5.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
		mCamera->setPerspectiveProjectionMatrix(glm::radians(90.0f), static_cast<f32>(mWidth) / static_cast<f32>(mHeight), 0.01f, 1000.0f);
		mCamera->setCameraSpeed(0.1f);

		// create uniform buffer
		sceneUb = new UniformBuffer(device);
		lightCountUb = new UniformBuffer(device);
		lightUb = new UniformBuffer(device);
		cameraUb = new UniformBuffer(device);
	}

	Scene::~Scene()
	{

	}

	void Scene::destroy()
	{
		for (auto& model : mModels) {

		}

		delete sceneUb;
		delete lightCountUb;
		delete lightUb;
		delete cameraUb;
		delete sceneDescritporSet;
		delete mCamera;
	}

	void Scene::loadModel(const std::string& path)
	{
		mModels.emplace_back(path, mDevice, mCommandBuffer, sceneDescritporSet);
	}

	void Scene::addDirectLight(vec3 color, f32 intensity, vec3 direction)
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

	void Scene::addPointLight(vec3 color, f32 intensity, vec3 position, f32 radius)
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

	void Scene::addSpotLight(vec3 color, f32 intensity, vec3 direction, vec3 position, f32 innerRadius, f32 outerRadius)
	{
		if (lightCountUbStruct.lightCount >= MAX_LIGHT_COUNT) {
			spdlog::warn("light count cannot more than {}", MAX_LIGHT_COUNT);
			return;
		}
		lightUbStruct.lights[lightCountUbStruct.lightCount].colorIntensity = { color, intensity };
		lightUbStruct.lights[lightCountUbStruct.lightCount].direction = { direction, 0.0 };
		lightUbStruct.lights[lightCountUbStruct.lightCount].positionType = { position, static_cast<f32>(LightType::SPOT_LIGHT) };
		lightUbStruct.lights[lightCountUbStruct.lightCount].radiusInnerOuter = { 0.0,innerRadius, outerRadius,0.0 };
		lightCountUbStruct.lightCount++;
	}

	void Scene::prepare()
	{
		sceneDescritporSet->createDescriptorPool();
		sceneDescritporSet->allocateDescriptors();

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

		sceneDescritporSet->updateDescriptorSet(&desc);

		for (auto& model : mModels) {
			model.updateDescriptors();
		}
	}

	void Scene::draw(Pipeline* pipeline) {
		for (auto& model : mModels) {
			model.draw(pipeline);
		}
	}

	std::vector<DescriptorSet> Scene::getDescriptors()
	{
		std::vector<DescriptorSet> sets{ *sceneDescritporSet, *mModels[0].getMaterialDescriptorSet(),*mModels[0].getMeshDescriptorSet() };
		return sets;
	}
	Camera* Scene::getMainCamera() const
	{
		return mCamera;
	}
}