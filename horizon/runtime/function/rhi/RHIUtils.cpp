#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon {

u32 GetStrideFromVertexAttributeDescription(VertexAttribFormat format, u32 portions) {
    assert(portions <= 4);
    u32 stride = 0;
    switch (format) {
    case Horizon::VertexAttribFormat::U8:
    case Horizon::VertexAttribFormat::S8:
    case Horizon::VertexAttribFormat::UN8:
    case Horizon::VertexAttribFormat::SN8:
        stride = portions * 1;
        break;
    case Horizon::VertexAttribFormat::U16:
    case Horizon::VertexAttribFormat::S16:
    case Horizon::VertexAttribFormat::UN16:
    case Horizon::VertexAttribFormat::SN16:
    case Horizon::VertexAttribFormat::F16:
        stride = portions * 2;
        break;
    case Horizon::VertexAttribFormat::U32:
    case Horizon::VertexAttribFormat::S32:
    case Horizon::VertexAttribFormat::F32:
        stride = portions * 4;
    default:
        break;
    }
    if(portions==3) stride=stride * 4 / 3;
    return stride;
}
} // namespace Horizon