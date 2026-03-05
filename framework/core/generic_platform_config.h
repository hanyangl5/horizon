#pragma once
#include "path.h"
namespace Horizon
{

class GenericPlatformConfig
{
  public:
    // Runtime path config for Android (from [android] in config.toml).
    struct AndroidPathConfig
    {
        bool use_external_files_dir = true;
        bool use_mesh_shader = true;
        Path external_assets_dir = "assets";
        Path external_shader_dir = "saved/shaders";
        Path external_log_dir = "logs";
        Path meshlet_dir = "saved/meshlet";
    };

    struct WindowsPathConfig
    {
        bool development_config = true;
        bool use_mesh_shader = true;
        Path assets_dir = "assets";
        Path shader_source_dir = "shaders";
        Path shader_ir_dir = "saved/shaders";
        Path log_dir = "logs";
        Path meshlet_dir = "saved/meshlet";
    };

  public:
    // Returns cached config; loaded once per config path.
    static const WindowsPathConfig &get_windows_path_config();
    static const AndroidPathConfig &get_android_path_config();
    // Unified runtime switch for mesh shader usage.
    static bool use_mesh_shader();
};
} // namespace Horizon
