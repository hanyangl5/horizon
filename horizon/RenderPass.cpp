#include "RenderPass.h"

namespace Horizon {

	RenderPass::RenderPass(std::shared_ptr<Device> device, const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo) :mDevice(device)
	{
		createRenderPass(attachmentsCreateInfo);
	}
	void RenderPass::createRenderPass(const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo)
	{
		u32 attachmentCount = attachmentsCreateInfo.size();

		std::vector<VkAttachmentDescription> attachmentsDesc(attachmentCount);

		for (u32 i = 0; i < attachmentsDesc.size(); i++) {
			attachmentsDesc[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentsDesc[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentsDesc[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
			attachmentsDesc[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentsDesc[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentsDesc[i].format = attachmentsCreateInfo[i].format;
			attachmentsDesc[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			if (attachmentsCreateInfo[i].usage & AttachmentUsageFlags::PRESENT_SRC) {
				if (attachmentsCreateInfo[i].usage & AttachmentUsageFlags::COLOR_ATTACHMENT) {
					attachmentsDesc[i].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				}
				else if (attachmentsCreateInfo[i].usage & AttachmentUsageFlags::DEPTH_STENCIL_ATTACHMENT) {
					attachmentsDesc[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				}
			}
			else if (attachmentsCreateInfo[i].usage & AttachmentUsageFlags::COLOR_ATTACHMENT) {
				attachmentsDesc[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			else if (attachmentsCreateInfo[i].usage & AttachmentUsageFlags::DEPTH_STENCIL_ATTACHMENT) {
				attachmentsDesc[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			else {
				spdlog::error("not an valid attachment");
			}
		}

		// if attachements have depth attachment, it should be the last attachment
		bool hasDepthAttachment = false;
		if (attachmentsCreateInfo[attachmentCount - 1].usage & AttachmentUsageFlags::DEPTH_STENCIL_ATTACHMENT) {
			hasDepthAttachment = true;
			attachmentsDesc[attachmentCount - 1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}

		
		std::vector<VkAttachmentReference> attachmentReferences(attachmentCount);
		for (u32 i = 0; i < attachmentReferences.size(); i++)
		{
			attachmentReferences[i].attachment = i;
			attachmentReferences[i].layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		}
		// set last attachment as depth
		if (hasDepthAttachment) {
			attachmentReferences[attachmentCount - 1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = hasDepthAttachment ? attachmentCount - 1 : attachmentCount;
		subpass.pColorAttachments = attachmentReferences.data();
		subpass.pDepthStencilAttachment = hasDepthAttachment ? &attachmentReferences[attachmentCount - 1] : nullptr;

		//
		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT ;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT ;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<u32>(attachmentsDesc.size());
		renderPassInfo.pAttachments = attachmentsDesc.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = dependencies.size();
		renderPassInfo.pDependencies = dependencies.data();

		printVkError(vkCreateRenderPass(mDevice->get(), &renderPassInfo, nullptr, &mRenderPass), "failed to create render pass");
	}

	RenderPass::~RenderPass()
	{
		vkDestroyRenderPass(mDevice->get(), mRenderPass, nullptr);
	}

	VkRenderPass RenderPass::get() const
	{
		return mRenderPass;
	}
}