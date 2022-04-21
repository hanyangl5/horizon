#include "Log.h"

#include "spdlog/sinks/stdout_color_sinks.h" // or "../stdout_sinks.h" if no colors needed

namespace Horizon {

	Log::Log()
	{
		m_logger = spdlog::stdout_color_mt("horizon logger");
		spdlog::set_default_logger(m_logger);
#ifndef NDEBUG
		spdlog::set_level(spdlog::level::debug);
#else
		spdlog::set_level(spdlog::level::info);
#endif // !NDEBUG

	}

	Log::~Log()
	{
		m_logger->flush();
		spdlog::drop_all();
	}

	void Log::CheckVulkanResult(VkResult _res) const noexcept
	{
		if (_res != VK_SUCCESS) {
			m_logger->error("vulkan result checking failed");
		}
	}

}