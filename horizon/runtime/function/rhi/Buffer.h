#pragma once

#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/RenderContext.h>

namespace Horizon
{
	namespace RHI
	{

		class Buffer
		{
		public:
			Buffer(const BufferCreateInfo &buffer_create_info) noexcept;
			virtual ~Buffer() noexcept = default;
			u32 GetBufferSize() noexcept;

		private:
			virtual void Destroy() noexcept = 0;

		protected:
			u32 m_usage;
			u32 m_size;
		};
	}
}