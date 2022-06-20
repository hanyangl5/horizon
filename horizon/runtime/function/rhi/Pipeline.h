#pragma once

namespace Horizon
{
	namespace RHI
	{

		enum class PipelineType
		{
			GRAPHICS = 0, COMPUTE
		};

		class Pipeline
		{
		public:
			Pipeline() noexcept;
			~Pipeline() noexcept;
			PipelineType GetType() const noexcept {
				return m_type;
			}

			void* GetHandle() noexcept {
				return handle;
			}
		private:
			PipelineType m_type;
			void* handle;
		};

		Pipeline::Pipeline() noexcept
		{
		}

		Pipeline::~Pipeline() noexcept
		{
		}
	}
}
