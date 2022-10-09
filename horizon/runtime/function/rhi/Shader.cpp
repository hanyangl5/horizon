#include "Shader.h"

#include <string>
#include <utility>
#include <algorithm>

#include <nlohmann/json.hpp>


namespace Horizon::RHI {

Shader::Shader(ShaderType type, const std::filesystem::path& rsd_path) noexcept : m_type(type), m_rsd_path(rsd_path) {

}

ShaderType Shader::GetType() const noexcept { return m_type; }

} // namespace Horizon::RHI