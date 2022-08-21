#include "ShaderProgram.h"

#include <utility>

namespace Horizon::RHI {

ShaderProgram::ShaderProgram(ShaderType type) noexcept : m_type(type) {}

ShaderType ShaderProgram::GetType() const noexcept { return m_type; }

const std::string &ShaderProgram::GetEntryPoint() const noexcept { return m_entry_point; }
} // namespace Horizon::RHI