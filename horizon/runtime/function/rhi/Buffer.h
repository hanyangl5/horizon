#pragma once

#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/RenderContext.h>

namespace Horizon {
	namespace RHI {

		class Buffer {
		public:
			Buffer() = default;
			virtual ~Buffer() = default;
			u32 GetBufferSize();
		private:
			virtual void Destroy() = 0;
		private:
			u32 m_buffer_usage;
			u32 m_buffer_size;
		};
	}
}