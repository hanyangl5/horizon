/*****************************************************************/ /**
 * \file   Log.cpp
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#include <spdlog/sinks/stdout_color_sinks.h> // or "../stdout_sinks.h" if no colors needed

#include "Log.h"

namespace Horizon {

Log::Log() noexcept {
    m_logger = spdlog::stdout_color_mt("log");
    spdlog::set_default_logger(m_logger);
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif // !NDEBUG
}

Log::~Log() noexcept {
    m_logger->flush();
    spdlog::drop_all();
}

void Log::CheckVulkanResult(VkResult _res, const char *func_name, int line) const noexcept {
    if (_res != VK_SUCCESS) {
        m_logger->error("[function: {}], [line: {}], vulkan error", func_name, line);
    }
    assert(_res == VK_SUCCESS);
}

} // namespace Horizon