#pragma once

#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/RenderContext.h>

namespace Horizon {
	namespace RHI {

		class Buffer {
		public:
			Buffer(const BufferCreateInfo& buffer_create_info);
			virtual ~Buffer() = default;
			u32 GetBufferSize();
		private:
			virtual void Destroy() = 0;
		protected:
			u32 m_usage;
			u32 m_size;
		};
	}
}