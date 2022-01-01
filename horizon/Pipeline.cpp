#include "Pipeline.h"
#include "Instance.h"
#include "Device.h"
#include "SwapChain.h"
#include "Surface.h"
#include "ShaderModule.h"
#include <array>
Pipeline::Pipeline(std::shared_ptr<Instance> instance,
	std::shared_ptr<Device> device,
	std::shared_ptr<Surface> surface,
	std::shared_ptr<SwapChain> swapchain):mInstance(instance),mDevice(device),mSurface(surface),mSwapChain(swapchain)
{

	Shader vs(mDevice->get(), "C:/Users/hylu/OneDrive/mycode/vulkan/shaders/basicvert.spv");
	Shader ps(mDevice->get(),"C:/Users/hylu/OneDrive/mycode/vulkan/shaders/basicvert.spv");

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

	// vertex shader
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = vs.get();
	shaderStages[0].pName = "main";

	//pixel shader
	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = ps.get();
	shaderStages[1].pName = "main";

}

Pipeline::~Pipeline()
{
}
