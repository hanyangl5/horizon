/*****************************************************************/ /**
                                                                     * \file   Log.cpp
                                                                     * \brief
                                                                     *
                                                                     * \author hylu
                                                                     * \date   November 2022
                                                                     *********************************************************************/

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

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h> // or "../stdout_sinks.h" if no colors needed

// Restore warnings
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "log.h"

#include <core/path.h>

namespace Horizon
{

Log::Log() noexcept
{
    m_logger = spdlog::stdout_color_mt("log");
    spdlog::set_default_logger(m_logger);
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif // !NDEBUG
    spdlog::flush_on(spdlog::level::info);
}

void Log::SetFileSink(const std::string &file_path) noexcept
{
    if (file_path.empty() || file_path == m_file_sink_path)
    {
        return;
    }

    const Path log_file(file_path);
    const Path log_dir = log_file.parent_path();
    if (!log_dir.empty() && !log_dir.exists())
    {
        log_dir.create_directories();
    }

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path, true);
    m_logger->sinks().push_back(file_sink);
    m_file_sink_path = file_path;
    m_logger->info("[SetFileSink] Logging to file: {}", m_file_sink_path);
}

Log::~Log() noexcept
{
    m_logger->flush();
    spdlog::drop_all();
}

} // namespace Horizon
