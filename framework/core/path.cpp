/*****************************************************************/ /**
                                                                     * \file   path.cpp
                                                                     * \brief  Simple path utility class implementation
                                                                     *
                                                                     * \author hylu
                                                                     * \date   December 2024
                                                                     *********************************************************************/

#include "path.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>

#include <core/log.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define stat _stat
#define S_IFDIR _S_IFDIR
#define S_IFREG _S_IFREG
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Horizon
{

Path::Path(const char *path) : m_path(path ? path : "")
{
    normalize();
}

Path::Path(const std::string &path) : m_path(path)
{
    normalize();
}

void Path::normalize()
{
    if (m_path.empty())
        return;

    char sep = get_separator();
    char other_sep = (sep == '/') ? '\\' : '/';

    // Replace other separator with native separator
    std::replace(m_path.begin(), m_path.end(), other_sep, sep);

    // Remove trailing separators (except for root paths)
#ifdef _WIN32
    // On Windows, preserve drive letter trailing separator (C:\)
    if (m_path.length() > 3 && m_path[1] == ':' && m_path[2] == sep)
    {
        // Keep C:\ as is
    }
    else
    {
        while (m_path.length() > 1 && m_path.back() == sep)
        {
            m_path.pop_back();
        }
    }
#else
    // On Unix, preserve root /
    while (m_path.length() > 1 && m_path.back() == sep)
    {
        m_path.pop_back();
    }
#endif

    // Collapse multiple separators
    std::string normalized;
    normalized.reserve(m_path.length());
    bool last_was_sep = false;
    for (char c : m_path)
    {
        if (c == sep)
        {
            if (!last_was_sep)
            {
                normalized += c;
                last_was_sep = true;
            }
        }
        else
        {
            normalized += c;
            last_was_sep = false;
        }
    }
    m_path = normalized;
}

char Path::get_separator()
{
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

Path Path::operator/(const char *other) const
{
    if (!other || *other == '\0')
        return *this;

    Path result = *this;
    result /= other;
    return result;
}

Path Path::operator/(const std::string &other) const
{
    if (other.empty())
        return *this;

    Path result = *this;
    result /= other;
    return result;
}

Path Path::operator/(const Path &other) const
{
    if (other.empty())
        return *this;

    Path result = *this;
    result /= other;
    return result;
}

Path &Path::operator/=(const char *other)
{
    if (!other || *other == '\0')
        return *this;

    if (!m_path.empty() && m_path.back() != get_separator())
    {
        m_path += get_separator();
    }
    m_path += other;
    normalize();
    return *this;
}

Path &Path::operator/=(const std::string &other)
{
    if (other.empty())
        return *this;

    if (!m_path.empty() && m_path.back() != get_separator())
    {
        m_path += get_separator();
    }
    m_path += other;
    normalize();
    return *this;
}

Path &Path::operator/=(const Path &other)
{
    if (other.empty())
        return *this;

    if (!m_path.empty() && m_path.back() != get_separator())
    {
        m_path += get_separator();
    }
    m_path += other.m_path;
    normalize();
    return *this;
}

std::string Path::string() const
{
    return m_path;
}

bool Path::exists() const
{
    if (m_path.empty())
        return false;

#ifdef _WIN32
    return _access(m_path.c_str(), 0) == 0;
#else
    return access(m_path.c_str(), F_OK) == 0;
#endif
}

bool Path::is_directory() const
{
    if (m_path.empty())
        return false;

#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(m_path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return false;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (stat(m_path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
#endif
}

bool Path::is_file() const
{
    if (m_path.empty())
        return false;

#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(m_path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return false;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat st;
    if (stat(m_path.c_str(), &st) != 0)
        return false;
    return S_ISREG(st.st_mode);
#endif
}

std::time_t Path::last_write_time() const
{
    if (m_path.empty())
        return 0;

#ifdef _WIN32
    struct _stat st;
    if (_stat(m_path.c_str(), &st) != 0)
        return 0;
    return st.st_mtime;
#else
    struct stat st;
    if (stat(m_path.c_str(), &st) != 0)
        return 0;
    return st.st_mtime;
#endif
}

bool Path::create_directories() const
{
    if (m_path.empty())
        return false;

    if (exists() && is_directory())
        return true;

    // Create parent directories first
    Path parent = parent_path();
    if (!parent.empty() && !parent.exists())
    {
        if (!parent.create_directories())
            return false;
    }

#ifdef _WIN32
    return _mkdir(m_path.c_str()) == 0 || errno == EEXIST;
#else
    return mkdir(m_path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

Path Path::parent_path() const
{
    if (m_path.empty())
        return Path();

    char sep = get_separator();
    size_t pos = m_path.find_last_of(sep);

#ifdef _WIN32
    // Handle Windows drive letters (C:\)
    if (pos == 2 && m_path.length() > 2 && m_path[1] == ':')
    {
        return Path(m_path.substr(0, 3));
    }
#endif

    if (pos == std::string::npos)
    {
        return Path(".");
    }

    if (pos == 0)
    {
        return Path(std::string(1, sep));
    }

    return Path(m_path.substr(0, pos));
}

std::string Path::filename() const
{
    if (m_path.empty())
        return "";

    char sep = get_separator();
    size_t pos = m_path.find_last_of(sep);

    if (pos == std::string::npos)
    {
        return m_path;
    }

    if (pos == m_path.length() - 1)
    {
        // Trailing separator, find previous one
        size_t prev_pos = m_path.find_last_of(sep, pos - 1);
        if (prev_pos == std::string::npos)
        {
            return m_path.substr(0, pos);
        }
        return m_path.substr(prev_pos + 1, pos - prev_pos - 1);
    }

    return m_path.substr(pos + 1);
}

std::string Path::extension() const
{
    std::string name = filename();
    if (name.empty())
        return "";

    size_t pos = name.find_last_of('.');
    if (pos == std::string::npos || pos == 0)
        return "";

    return name.substr(pos);
}

std::string Path::stem() const
{
    std::string name = filename();
    if (name.empty())
        return "";

    size_t pos = name.find_last_of('.');
    if (pos == std::string::npos || pos == 0)
        return name;

    return name.substr(0, pos);
}

std::string Path::generic_string() const
{
    std::string result = m_path;
    char sep = get_separator();
    if (sep == '\\')
    {
        std::replace(result.begin(), result.end(), '\\', '/');
    }
    return result;
}

bool Path::is_absolute() const
{
    if (m_path.empty())
        return false;

#ifdef _WIN32
    // On Windows: C:\ or \\server\share
    if (m_path.length() >= 2 && m_path[1] == ':')
        return true;
    if (m_path.length() >= 2 && m_path[0] == '\\' && m_path[1] == '\\')
        return true;
    return false;
#else
    // On Unix: starts with /
    return !m_path.empty() && m_path[0] == '/';
#endif
}

// Helper functions
bool exists(const Path &path)
{
    return path.exists();
}

bool is_directory(const Path &path)
{
    return path.is_directory();
}

bool is_file(const Path &path)
{
    return path.is_file();
}

std::time_t last_write_time(const Path &path)
{
    return path.last_write_time();
}

bool create_directories(const Path &path)
{
    return path.create_directories();
}

std::vector<char> ReadFile(const char *path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        LOG_ERROR("failed to open shader file: {}", path);
        return {};
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}
} // namespace Horizon
