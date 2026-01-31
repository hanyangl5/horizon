/*****************************************************************/ /**
 * \file   path.h
 * \brief  Simple path utility class to replace std::filesystem
 * 
 * \author hylu
 * \date   December 2024
 *********************************************************************/

#pragma once

#include <string>
#include <ctime>

namespace Horizon {

class Path {
  public:
    Path() = default;
    Path(const char *path);
    Path(const std::string &path);
    Path(const Path &other) = default;
    Path(Path &&other) noexcept = default;
    Path &operator=(const Path &other) = default;
    Path &operator=(Path &&other) noexcept = default;

    // Path operations
    Path operator/(const char *other) const;
    Path operator/(const std::string &other) const;
    Path operator/(const Path &other) const;
    Path &operator/=(const char *other);
    Path &operator/=(const std::string &other);
    Path &operator/=(const Path &other);

    // String conversion
    std::string string() const;
    const std::string &str() const { return m_path; }
    const char *c_str() const { return m_path.c_str(); }

    // File system operations
    bool exists() const;
    bool is_directory() const;
    bool is_file() const;
    std::time_t last_write_time() const;
    bool create_directories() const;

    // Path queries
    Path parent_path() const;
    std::string filename() const;
    std::string extension() const;
    std::string stem() const; // filename without extension
    std::string generic_string() const; // string with forward slashes
    bool is_absolute() const;
    bool empty() const { return m_path.empty(); }

    // Comparison
    bool operator==(const Path &other) const { return m_path == other.m_path; }
    bool operator!=(const Path &other) const { return m_path != other.m_path; }
    bool operator<(const Path &other) const { return m_path < other.m_path; }

  private:
    std::string m_path;
    void normalize();
    static char get_separator();
};

// Helper functions
bool exists(const Path &path);
bool is_directory(const Path &path);
bool is_file(const Path &path);
std::time_t last_write_time(const Path &path);
bool create_directories(const Path &path);

} // namespace Horizon
