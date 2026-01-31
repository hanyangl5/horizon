/*****************************************************************/ /**
                                                                     * \file   Log.h
                                                                     * \brief  log functions
                                                                     *
                                                                     * \author hylu
                                                                     * \date   November 2022
                                                                     *********************************************************************/

#pragma once

// Suppress warnings from spdlog (third-party library)
#if defined(_MSC_VER)
#pragma warning(push, 0)
#pragma warning(disable : 4068) // unknown pragma
#pragma warning(disable : 4996) // deprecated functions
#pragma warning(disable : 4267) // conversion from size_t to smaller type
#pragma warning(disable : 4244) // conversion from larger to smaller type
#pragma warning(disable : 4458) // declaration hides class member
#pragma warning(disable : 4456) // declaration hides previous local declaration
#pragma warning(disable : 4459) // declaration hides global declaration
#pragma warning(disable : 4702) // unreachable code
#pragma warning(disable : 4127) // conditional expression is constant
#pragma warning(disable : 4100) // unreferenced formal parameter
#pragma warning(disable : 4505) // unreferenced local function
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wpadded"
#pragma GCC diagnostic ignored "-Wredundant-decls"
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wswitch-enum"
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wunreachable-code"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
#pragma GCC diagnostic ignored "-Wreserved-identifier"
#pragma GCC diagnostic ignored "-Wdocumentation"
#pragma GCC diagnostic ignored "-Wdocumentation-unknown-command"
#endif
#endif

#include <spdlog/spdlog.h>

// Restore warnings
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <core/definations.h>
#include <core/public_singleton.h>

namespace Horizon
{

class Log : public PublicSingleton<Log>
{
  public:
    enum loglevel : u8
    {
        debug,
        info,
        warn,
        error
    };

  public:
    Log() noexcept;
    ~Log() noexcept;

    template <typename... args> inline void Debug(args &&..._args) const noexcept
    {
        m_logger->debug(std::forward<args>(_args)...);
    }

    template <typename... args> void Info(args &&..._args) const noexcept
    {
        m_logger->info(std::forward<args>(_args)...);
    }

    template <typename... args> void Warn(args &&..._args) const noexcept
    {
        m_logger->warn(std::forward<args>(_args)...);
    }

    template <typename... args> void Error(args &&..._args) const noexcept
    {
        m_logger->error(std::forward<args>(_args)...);
    }

  private:
    std::shared_ptr<spdlog::logger> m_logger;
};

#define LOG_DEBUG(...) Log::GetInstance().Debug("[" + std::string(__FUNCTION__) + "] " + __VA_ARGS__);

#define LOG_INFO(...) Log::GetInstance().Info("[" + std::string(__FUNCTION__) + "] " + __VA_ARGS__);

#define LOG_WARN(...) Log::GetInstance().Warn("[" + std::string(__FUNCTION__) + "] " + __VA_ARGS__);

#define LOG_ERROR(...) Log::GetInstance().Error("[" + std::string(__FUNCTION__) + "] " + __VA_ARGS__);

#define CHECK_VK_RESULT(res) CheckVulkanResult(res, __FUNCTION__, __LINE__);

} // namespace Horizon
