#include "ShaderProgram.h"

namespace Horizon::RHI {

ShaderProgram::ShaderProgram(ShaderType type,
                             const std::string &entry_point) noexcept
    : m_type(type), m_entry_point(entry_point) {}

ShaderType ShaderProgram::GetType() const noexcept { return m_type; }
const std::string &ShaderProgram::GetEntryPoint() const noexcept {
    return m_entry_point;
}
} // namespace Horizon::RHI