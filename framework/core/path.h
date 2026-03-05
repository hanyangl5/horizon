/*****************************************************************/ /**
                                                                     * \file   path.h
                                                                     * \brief  Path utility and runtime resource paths
                                                                     *
                                                                     * \author hylu
                                                                     * \date   December 2024
                                                                     *********************************************************************/

#pragma once

#include <ctime>
#include <string>
#include <string_view>
#include <vector>

namespace Horizon
{
class Path
{
  public:
    Path() = default;
    Path(const char *path);
    Path(const std::string &path);
    Path(std::string_view path);
    Path(const Path &other) = default;
    Path(Path &&other) noexcept = default;
    Path &operator=(const Path &other) = default;
    Path &operator=(Path &&other) noexcept = default;

    Path operator/(const char *other) const;
    Path operator/(const std::string &other) const;
    Path operator/(const Path &other) const;
    Path &operator/=(const char *other);
    Path &operator/=(const std::string &other);
    Path &operator/=(const Path &other);

    std::string string() const;
    const std::string &str() const
    {
        return m_path;
    }
    const char *c_str() const
    {
        return m_path.c_str();
    }

    bool exists() const;
    bool is_directory() const;
    bool is_file() const;
    std::time_t last_write_time() const;
    bool create_directories() const;

    static void set_project_root(const Path &project_root);
    static Path project_root();
    static Path asset_directory();
    static Path shader_source_directory();
    static Path shader_directory(); // alias for shader_ir_directory
    static Path log_file_path();
    static Path meshlet_cache_path();
    static void resolve_resource_paths(Path *shader_dir, Path *asset_dir = nullptr);

    static std::vector<char> read_file(const char *path);

    Path parent_path() const;
    std::string filename() const;
    std::string extension() const;
    std::string stem() const;
    std::string generic_string() const;
    bool is_absolute() const;
    bool empty() const
    {
        return m_path.empty();
    }

    bool operator==(const Path &other) const
    {
        return m_path == other.m_path;
    }
    bool operator!=(const Path &other) const
    {
        return m_path != other.m_path;
    }
    bool operator<(const Path &other) const
    {
        return m_path < other.m_path;
    }

  private:
    std::string m_path;
    void normalize();
    static char get_separator();
};

} // namespace Horizon
