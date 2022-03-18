#include "Scene.h"
#include "UniformBuffer.h"

Scene::Scene(Device* device, CommandBuffer* commandBuffer) :mDevice(device), mCommandBuffer(commandBuffer)
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

	mCamera = new Camera(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	mCamera->SetProjectionMatrix(glm::perspective(75.0f, 4.0f / 3.0f, 0.01f, 1000.0f));

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
	delete sceneDescritporSet;
	delete mCamera;
}

void Scene::loadModel(const std::string& path)
{
	mModels.emplace_back(path, mDevice, mCommandBuffer, sceneDescritporSet);
}

void Scene::addDirectLight(glm::vec3 color, f32 intensity, glm::vec3 direction)
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

void Scene::addPointLight(glm::vec3 color, f32 intensity, glm::vec3 position, f32 radius)
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

void Scene::addSpotLight(glm::vec3 color, f32 intensity, glm::vec3 direction, glm::vec3 position, f32 innerRadius, f32 outerRadius)
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
	sceneUbStruct.view = mCamera->GetViewMatrix();
	sceneUbStruct.projection = mCamera->getProjectionMatrix();
	camaeraUbStruct.cameraPos = mCamera->Position;

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
