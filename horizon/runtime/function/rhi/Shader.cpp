#include "Shader.h"

#include <utility>

namespace Horizon::RHI {

Shader::Shader(ShaderType type) noexcept : m_type(type) {}

ShaderType Shader::GetType() const noexcept { return m_type; }

const std::string &Shader::GetEntryPoint() const noexcept { return m_entry_point; }
} // namespace Horizon::RHI