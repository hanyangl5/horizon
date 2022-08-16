#include "Pipeline.h"

namespace Horizon::RHI {

Pipeline::Pipeline() noexcept {}

Pipeline::~Pipeline() noexcept {}

PipelineType Pipeline::GetType() const noexcept { return m_create_info.type; }

} // namespace Horizon::RHI
