#include "Scene.h"
#include "UniformBuffer.h"

Scene::Scene(Device* device, CommandBuffer* commandBuffer) :mDevice(device), mCommandBuffer(commandBuffer)
{

	DescriptorSetInfo sceneDescriptorSetInfo{};
	// vp mat
	sceneDescriptorSetInfo.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	sceneDescriptorSetInfo.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS);

	sceneDescritporSet = new DescriptorSet(mDevice, &sceneDescriptorSetInfo);
	//sceneDescritporSet->createDescriptorPool();
	//sceneDescritporSet->allocateDescriptors();

	mCamera = new Camera(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	mCamera->SetProjectionMatrix(glm::perspective(75.0f, 4.0f / 3.0f, 0.01f, 1000.0f));

	// create uniform buffer
	sceneUbo = new UniformBuffer(device);
	sceneUbo2 = new UniformBuffer(device);

	// update ubo data
	sceneUboStruct.view = mCamera->GetViewMatrix();
	sceneUboStruct.projection = mCamera->getProjectionMatrix();
	sceneUboStruct2.a = 1.0;
	sceneUboStruct2.b = 2.0;

}

Scene::~Scene()
{
}

void Scene::destroy()
{
	delete sceneUbo; 
	delete sceneDescritporSet;
	delete mCamera;
}

void Scene::loadModel(const std::string& path)
{
	mModels.emplace_back(path, mDevice, mCommandBuffer, sceneDescritporSet);
}

void Scene::prepare()
{
	sceneDescritporSet->createDescriptorPool();
	sceneDescritporSet->allocateDescriptors();

	sceneUbo->update(&sceneUboStruct, sizeof(sceneUboStruct));
	sceneUbo2->update(&sceneUboStruct2, sizeof(sceneUboStruct2));

	DescriptorSetUpdateDesc desc;
	desc.addBinding(0, sceneUbo);
	desc.addBinding(1, sceneUbo2);

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
	std::vector<DescriptorSet> sets{ *sceneDescritporSet, *mModels[0].getDescriptorSet() };
	return sets;
}
