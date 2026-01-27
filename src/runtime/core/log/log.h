#pragma once

#include <memory>
#include <string>

#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>

#include <runtime/core/math/Math.h>
#include <runtime/core/singleton/public_singleton.h>

namespace Horizon {
class Log : public PublicSingleton<Log> {
  public:
    enum loglevel : u8 { debug, info, warn, error, fatal };

    Log();
    ~Log() override;
    Log(const Log &) = delete;
    Log(Log &&) = delete;
    Log &operator=(const Log &) = delete;
    Log &operator=(Log &&) = delete;

    template <typename... args> inline void Debug(args &&..._args) const noexcept {
        m_logger->debug(std::forward<args>(_args)...);
    }

    template <typename... args> inline void Info(args &&..._args) const noexcept {
        m_logger->info(std::forward<args>(_args)...);
    }

    template <typename... args> inline void Warn(args &&..._args) const noexcept {
        m_logger->warn(std::forward<args>(_args)...);
    }

    template <typename... args> inline void Error(args &&..._args) const noexcept {
        m_logger->error(std::forward<args>(_args)...);
    }

    void CheckVulkanResult(VkResult _res, const char *function, u32 line) const noexcept;

  private:
    std::shared_ptr<spdlog::logger> m_logger;
};

#define LOG_DEBUG(...) Log::GetInstance().Debug("[" + std::string(__FUNCTION__) + "] " + __VA_ARGS__);

#define LOG_INFO(...) Log::GetInstance().Info("[" + std::string(__FUNCTION__) + "] " + __VA_ARGS__);

#define LOG_WARN(...) Log::GetInstance().Warn("[" + std::string(__FUNCTION__) + "] " + __VA_ARGS__);

#define LOG_ERROR(...) Log::GetInstance().Error("[" + std::string(__FUNCTION__) + "] " + __VA_ARGS__);

#define CHECK_VK_RESULT(res) Log::GetInstance().CheckVulkanResult(res, __FUNCTION__, __LINE__);
} // namespace Horizon
