#include "generic_platform_config.h"

#include <fstream>
#include <string>
#include <string_view>

#ifdef __ANDROID__
#include <core/android/android_native.h>
#endif

namespace Horizon
{

namespace
{

std::string_view trim(std::string_view value)
{
    constexpr std::string_view whitespace = " \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string_view::npos)
    {
        return {};
    }
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

std::string_view strip_comment(std::string_view value)
{
    const size_t hash = value.find('#');
    return (hash == std::string_view::npos) ? value : value.substr(0, hash);
}

std::string_view unquote(std::string_view value)
{
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

bool parse_bool(std::string_view value)
{
    const std::string_view cleaned = trim(unquote(value));
    return cleaned == "true" || cleaned == "1";
}

Path parse_path(std::string_view value)
{
    return Path(unquote(trim(value)));
}

template <typename Fn>
void parse_toml_section(const Path &config_path, std::string_view section_name, Fn &&on_entry)
{
    // Lightweight line-based parser: enough for flat key/value config sections.
    std::ifstream file(config_path.c_str());
    if (!file.is_open())
    {
        return;
    }

    bool in_section = false;
    std::string line;
    while (std::getline(file, line))
    {
        std::string_view cleaned = trim(strip_comment(line));
        if (cleaned.empty())
        {
            continue;
        }

        if (cleaned.front() == '[' && cleaned.back() == ']')
        {
            in_section = (cleaned == section_name);
            continue;
        }

        if (!in_section)
        {
            continue;
        }

        const size_t eq = cleaned.find('=');
        if (eq == std::string_view::npos)
        {
            continue;
        }

        const std::string_view key = trim(cleaned.substr(0, eq));
        const std::string_view value = trim(cleaned.substr(eq + 1));
        on_entry(key, value);
    }
}

Path get_host_config_path()
{
    const Path sample_root = Path::project_root();
    if (!sample_root.empty())
    {
        const Path from_sample = sample_root / ".." / ".." / "framework" / "config" / "config.toml";
        if (from_sample.exists())
        {
            return from_sample;
        }
    }

    const Path from_cwd("framework/config/config.toml");
    if (from_cwd.exists())
    {
        return from_cwd;
    }

    return Path();
}

#ifdef __ANDROID__
Path get_android_config_path()
{
    const std::string &external_files_dir = GetAndroidExternalFilesDir();
    if (external_files_dir.empty())
    {
        return Path();
    }

    return Path(external_files_dir) / "config" / "config.toml";
}
#endif

void parse_android_path_config(const Path &config_path, GenericPlatformConfig::AndroidPathConfig &cfg)
{
    parse_toml_section(config_path, "[android]", [&](std::string_view key, std::string_view value) {
        if (key == "use_external_files_dir")
        {
            cfg.use_external_files_dir = parse_bool(value);
        }
        else if (key == "use_mesh_shader")
        {
            cfg.use_mesh_shader = parse_bool(value);
        }
        else if (key == "external_assets_dir" || key == "assets_dir")
        {
            cfg.external_assets_dir = parse_path(value);
        }
        else if (key == "external_shader_dir" || key == "shader_dir")
        {
            cfg.external_shader_dir = parse_path(value);
        }
        else if (key == "external_log_dir")
        {
            cfg.external_log_dir = parse_path(value);
        }
        else if (key == "meshlet_path")
        {
            cfg.meshlet_dir = parse_path(value);
        }
    });
}

void parse_windows_path_config(const Path &config_path, GenericPlatformConfig::WindowsPathConfig &cfg)
{
    parse_toml_section(config_path, "[windows]", [&](std::string_view key, std::string_view value) {
        if (key == "development_config")
        {
            cfg.development_config = parse_bool(value);
        }
        else if (key == "use_mesh_shader")
        {
            cfg.use_mesh_shader = parse_bool(value);
        }
        else if (key == "assets_dir")
        {
            cfg.assets_dir = parse_path(value);
        }
        else if (key == "shader_source_dir")
        {
            cfg.shader_source_dir = parse_path(value);
        }
        else if (key == "shader_ir_dir")
        {
            cfg.shader_ir_dir = parse_path(value);
        }
        else if (key == "log_dir")
        {
            cfg.log_dir = parse_path(value);
        }
        else if (key == "meshlet_path")
        {
            cfg.meshlet_dir = parse_path(value);
        }
    });
}

} // namespace

const GenericPlatformConfig::AndroidPathConfig &GenericPlatformConfig::get_android_path_config()
{
    static GenericPlatformConfig::AndroidPathConfig cfg{};
    static bool loaded = false;
    static Path loaded_from;

#ifdef __ANDROID__
    const Path config_path = get_android_config_path();
    if (!loaded || loaded_from != config_path)
    {
        cfg = GenericPlatformConfig::AndroidPathConfig{};
        if (!config_path.empty())
        {
            parse_android_path_config(config_path, cfg);
        }
        loaded = true;
        loaded_from = config_path;
    }
#else
    if (!loaded)
    {
        loaded = true;
    }
#endif

    return cfg;
}

const GenericPlatformConfig::WindowsPathConfig &GenericPlatformConfig::get_windows_path_config()
{
    static GenericPlatformConfig::WindowsPathConfig cfg{};
    static bool loaded = false;
    static Path loaded_from;

    const Path config_path = get_host_config_path();
    if (!loaded || loaded_from != config_path)
    {
        cfg = GenericPlatformConfig::WindowsPathConfig{};
        if (!config_path.empty())
        {
            parse_windows_path_config(config_path, cfg);
        }
        loaded = true;
        loaded_from = config_path;
    }

    return cfg;
}

bool GenericPlatformConfig::use_mesh_shader()
{
#ifdef __ANDROID__
    const AndroidPathConfig &cfg = get_android_path_config();
    return cfg.use_mesh_shader;
#else
    const WindowsPathConfig &cfg = get_windows_path_config();
    return cfg.use_mesh_shader;
#endif
}

} // namespace Horizon
