#include <gtest/gtest.h>

#include "RHI/RingBuffer.h"

namespace
{
template <typename T>
T* fakeHandle(uintptr_t value)
{
    return (T*)value;
}
} // namespace

// Verifies that GPU ring buffer allocations honor alignment overrides and wrap to the beginning when needed.
TEST(RHIRingBufferTest, RingBufferOffsetsHonorAlignmentAndWrap)
{
    Buffer buffer = {};

    GPURingBuffer ring = {
        .pBuffer = &buffer,
        .bufferAlignment = 16,
        .maxBufferSize = 64,
    };

    const GPURingBufferOffset first = getGPURingBufferOffset(&ring, 1);
    EXPECT_EQ(first.pBuffer, &buffer);
    EXPECT_EQ(first.offset, 0u);
    EXPECT_EQ(ring.currentBufferOffset, 16u);

    const GPURingBufferOffset second = getGPURingBufferOffset(&ring, 17);
    EXPECT_EQ(second.offset, 16u);
    EXPECT_EQ(ring.currentBufferOffset, 48u);

    const GPURingBufferOffset third = getGPURingBufferOffset(&ring, 17, 32);
    EXPECT_EQ(third.offset, 0u);
    EXPECT_EQ(ring.currentBufferOffset, 32u);
}

// Verifies that resetting the GPU ring buffer restarts allocations from offset zero.
TEST(RHIRingBufferTest, ResetClearsCurrentOffset)
{
    Buffer buffer = {};

    GPURingBuffer ring = {
        .pBuffer = &buffer,
        .bufferAlignment = 16,
        .maxBufferSize = 128,
        .currentBufferOffset = 64,
    };

    resetGPURingBuffer(&ring);

    EXPECT_EQ(ring.currentBufferOffset, 0u);

    const GPURingBufferOffset allocation = getGPURingBufferOffset(&ring, 8);
    EXPECT_EQ(allocation.offset, 0u);
    EXPECT_EQ(ring.currentBufferOffset, 16u);
}

// Verifies that the GPU command ring advances command and fence indices within a pool and cycles to the next pool on request.
TEST(RHIRingBufferTest, CommandRingReturnsSequentialElementsAcrossPools)
{
    GpuCmdRing ring = {
        .poolIndex = UINT32_MAX,
        .cmdIndex = UINT32_MAX,
        .fenceIndex = UINT32_MAX,
        .poolCount = 2,
        .cmdPerPoolCount = 3,
    };

    ring.pCmdPools[0] = fakeHandle<CmdPool>(0x1000);
    ring.pCmdPools[1] = fakeHandle<CmdPool>(0x2000);

    ring.pCmds[0][0] = fakeHandle<Cmd>(0x1010);
    ring.pCmds[0][1] = fakeHandle<Cmd>(0x1020);
    ring.pCmds[0][2] = fakeHandle<Cmd>(0x1030);
    ring.pCmds[1][0] = fakeHandle<Cmd>(0x2010);
    ring.pCmds[1][1] = fakeHandle<Cmd>(0x2020);
    ring.pCmds[1][2] = fakeHandle<Cmd>(0x2030);

    ring.pFences[0][0] = fakeHandle<Fence>(0x1100);
    ring.pFences[0][1] = fakeHandle<Fence>(0x1200);
    ring.pFences[0][2] = fakeHandle<Fence>(0x1300);
    ring.pFences[1][0] = fakeHandle<Fence>(0x2100);
    ring.pFences[1][1] = fakeHandle<Fence>(0x2200);
    ring.pFences[1][2] = fakeHandle<Fence>(0x2300);

    ring.pSemaphores[0][0] = fakeHandle<Semaphore>(0x1110);
    ring.pSemaphores[0][1] = fakeHandle<Semaphore>(0x1210);
    ring.pSemaphores[0][2] = fakeHandle<Semaphore>(0x1310);
    ring.pSemaphores[1][0] = fakeHandle<Semaphore>(0x2110);
    ring.pSemaphores[1][1] = fakeHandle<Semaphore>(0x2210);
    ring.pSemaphores[1][2] = fakeHandle<Semaphore>(0x2310);

    const GpuCmdRingElement first = getNextGpuCmdRingElement(&ring, true, 2);
    EXPECT_EQ(first.pCmdPool, ring.pCmdPools[0]);
    EXPECT_EQ(first.pCmds, &ring.pCmds[0][0]);
    EXPECT_EQ(first.pFence, ring.pFences[0][0]);
    EXPECT_EQ(first.pSemaphore, ring.pSemaphores[0][0]);
    EXPECT_EQ(ring.poolIndex, 0u);
    EXPECT_EQ(ring.cmdIndex, 2u);
    EXPECT_EQ(ring.fenceIndex, 1u);

    const GpuCmdRingElement second = getNextGpuCmdRingElement(&ring, false, 1);
    EXPECT_EQ(second.pCmdPool, ring.pCmdPools[0]);
    EXPECT_EQ(second.pCmds, &ring.pCmds[0][2]);
    EXPECT_EQ(second.pFence, ring.pFences[0][1]);
    EXPECT_EQ(second.pSemaphore, ring.pSemaphores[0][1]);
    EXPECT_EQ(ring.cmdIndex, 3u);
    EXPECT_EQ(ring.fenceIndex, 2u);

    const GpuCmdRingElement third = getNextGpuCmdRingElement(&ring, true, 1);
    EXPECT_EQ(third.pCmdPool, ring.pCmdPools[1]);
    EXPECT_EQ(third.pCmds, &ring.pCmds[1][0]);
    EXPECT_EQ(third.pFence, ring.pFences[1][0]);
    EXPECT_EQ(third.pSemaphore, ring.pSemaphores[1][0]);
    EXPECT_EQ(ring.poolIndex, 1u);
    EXPECT_EQ(ring.cmdIndex, 1u);
    EXPECT_EQ(ring.fenceIndex, 1u);
}
