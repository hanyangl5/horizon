#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "Device.h"
#include "SwapChain.h"
#include "RenderPass.h"

namespace Horizon {
	class Framebuffer
	{
	public:
		Framebuffer(std::shared_ptr<Device> device, const std::vector<AttachmentCreateInfo>& attachment_create_info, RenderContext& render_context, std::shared_ptr<SwapChain> swap_chain = nullptr);
		~Framebuffer();
		VkFramebuffer Get() const noexcept;
		VkFramebuffer Get(u32 index) const noexcept;
		VkRenderPass getRenderPass() const noexcept;
		std::shared_ptr<AttachmentDescriptor> getDescriptorImageInfo(u32 attachment_index);
		std::vector<VkImage> getPresentImages();
		u32 getColorAttachmentCount();
		std::vector<VkClearValue> getClearValues();
	private:
		void createFrameBuffer(u32 width, u32 height, u32 imag_count, std::shared_ptr<SwapChain> swap_chain = nullptr);
		void createAttachmentsResources(const std::vector<AttachmentCreateInfo>& attachment_create_info);
	private:
		RenderContext& m_render_context;
		bool m_has_depth_attachment = false;
		std::shared_ptr<Device> m_device = nullptr;
		std::shared_ptr<RenderPass> m_render_pass;
		std::vector<VkFramebuffer> m_framebuffer;
		// Shared sampler used for all color attachments
		VkSampler m_sampler;
		std::vector<Attachment> m_frame_buffer_attachments;

	};
}
