#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon {

u32 GetStrideFromVertexAttributeDescription(VertexAttribFormat format,
                                            u32 portions) {
    u32 stride = 0;
    switch (format) {
    case Horizon::VertexAttribFormat::U8:
    case Horizon::VertexAttribFormat::F8:
    case Horizon::VertexAttribFormat::NF8:
        stride = portions * 8;
        break;
    case Horizon::VertexAttribFormat::U16:
    case Horizon::VertexAttribFormat::F16:
    case Horizon::VertexAttribFormat::NF16:
        stride = portions * 16;
        break;
    case Horizon::VertexAttribFormat::U32:
    case Horizon::VertexAttribFormat::F32:
    case Horizon::VertexAttribFormat::NF32:
        stride = portions * 32;
        break;
    default:
        break;
    }
    return stride / 8;
}
} // namespace Horizon