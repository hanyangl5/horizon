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

	std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos{};

	// vertex shader
	pipelineShaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	pipelineShaderStageCreateInfos[0].module = vs.get();
	pipelineShaderStageCreateInfos[0].pName = "main";

	//pixel shader
	pipelineShaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipelineShaderStageCreateInfos[1].module = ps.get();
	pipelineShaderStageCreateInfos[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr; // Optional

	//VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	//inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	//inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	//inputAssembly.primitiveRestartEnable = VK_FALSE;
	//VkViewport viewport{};
	//viewport.x = 0.0f;
	//viewport.y = 0.0f;
	//viewport.width = static_cast<f32>(mSwapChain->getExtent().width);
	//viewport.height = static_cast<f32>(mSwapChain->getExtent().height);
	//viewport.minDepth = 0.0f;
	//viewport.maxDepth = 1.0f;
}

Pipeline::~Pipeline()
{
}
