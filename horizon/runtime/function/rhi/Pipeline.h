#pragma once

#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon
{
	namespace RHI
	{

		class Pipeline
		{
		public:
			Pipeline() noexcept;
			~Pipeline() noexcept;

			PipelineType GetType() const noexcept;


		private:
			PipelineType m_type;
		};

	}
}
