#include "metal_shader.h"

namespace Horizon::Backend
{

MetalShader::MetalShader(ShaderType type, const char *entry_point, MTL::Library *library,
                         MTL::Function *function) noexcept
    : Shader(type, entry_point), m_library(library), m_function(function)
{
}

MetalShader::~MetalShader() noexcept
{
    if (m_function != nil)
    {
        m_function->release();
        m_function = nil;
    }
    if (m_library != nil)
    {
        m_library->release();
        m_library = nil;
    }
}

} // namespace Horizon::Backend
