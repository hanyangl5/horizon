#include "shader.h"

namespace Horizon::Backend
{

Shader::Shader(ShaderType type, const char *entry_point) noexcept : m_type(type)
{
    m_entry_point = std::string(entry_point);
}

ShaderType Shader::GetType() const noexcept
{
    return m_type;
}

} // namespace Horizon::Backend