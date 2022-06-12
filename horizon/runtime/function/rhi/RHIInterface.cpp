#include "RHIInterface.h"


namespace Horizon {
	namespace RHI {
		RHIInterface::RHIInterface() noexcept
		{
			m_thread_pool = std::make_unique<ThreadPool>(std::thread::hardware_concurrency());
			m_shader_compiler = std::make_shared<ShaderCompiler>();
		}
	}
}