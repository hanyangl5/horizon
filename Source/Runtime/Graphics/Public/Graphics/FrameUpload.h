/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IContainer.h"
#include "Graphics/RenderContext.h"

namespace hz
{
// A partition is writable from BeginFrame until EndFrame. Seal it with the last submission that reads it.
class FrameUpload
{
public:
    FrameUpload(RenderContext& Context, uint32_t FrameCount, uint32_t BytesPerFrame = 65536);
    void             BeginFrame(uint32_t FrameIndex);
    void             EndFrame(SubmitHandle Submission);
    bool             Upload(const void* Data, uint32_t Size, uint32_t& ByteOffset);
    BufferSrv        UploadStructured(const void* Data, uint32_t Stride);
    const GPUBuffer& GetBuffer() const { return Buffer; }

private:
    RenderContext&      Context;
    GPUBuffer           Buffer;
    Array<SubmitHandle> Submissions;
    uint32_t            Capacity;
    uint32_t            Partition = UINT32_MAX;
    uint32_t            Used = 0;
    uint32_t            ViewCount = 0;
};
} // namespace hz
