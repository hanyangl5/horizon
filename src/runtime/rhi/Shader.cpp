#include "Shader.h"

#include <algorithm>
#include <utility>

namespace Horizon::Backend {

Shader::Shader(ShaderType type, const std::filesystem::path &rsd_path) noexcept : m_type(type), m_rsd_path(rsd_path) {}

ShaderType Shader::GetType() const noexcept { return m_type; }

} // namespace Horizon::Backend