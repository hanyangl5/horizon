#include "RHIInterface.h"
namespace Horizon {
	namespace RHI {
		RHIInterface::RHIInterface() noexcept
		{
			m_shader_compiler = std::make_shared<ShaderCompiler>();
		}
	}
}