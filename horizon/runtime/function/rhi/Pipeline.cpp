#include "Pipeline.h"

namespace Horizon{
    namespace RHI
    {
        
		Pipeline::Pipeline() noexcept
		{

		}

		Pipeline::~Pipeline() noexcept
		{
            
		}

		PipelineType Pipeline::GetType() const noexcept
		{
			return m_type;
		}

    } // namespace RHI
    
}