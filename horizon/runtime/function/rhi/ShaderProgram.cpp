#include "ShaderProgram.h"

#include <utility>

namespace Horizon::RHI {

ShaderProgram::ShaderProgram(ShaderType type, std::string entry_point) noexcept
    : m_type(type), m_entry_point(std::move(entry_point)) {}

ShaderType ShaderProgram::GetType() const noexcept { return m_type; }
const std::string &ShaderProgram::GetEntryPoint() const noexcept {
    return m_entry_point;
}
} // namespace Horizon::RHI