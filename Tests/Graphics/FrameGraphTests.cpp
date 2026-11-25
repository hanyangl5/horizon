#include <gtest/gtest.h>

#include "Graphics/FrameGraph.h"
#include "FrameGraph/FrameGraphInternal.h"

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

// Compile-only graphs: no RenderContext, so no GPU objects are created.
class FrameGraphTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(InitMemoryAlloc(nullptr));
        Graph = hz::InitFrameGraph(nullptr);
    }
    void TearDown() override
    {
        hz::ExitFrameGraph(Graph);
        ExitMemoryAlloc();
    }
    static void Nothing(const hz::FrameGraphContext&, void*) {}
    hz::FrameGraphTexture Texture(hz::Format Format = hz::Format::R8G8B8A8_UNORM, uint32_t SizeShift = 0, bool History = false)
    {
        return hz::AddFrameGraphTexture(Graph, { .Texture = { .Format = Format }, .SizeShift = SizeShift, .History = History });
    }
    hz::FrameGraphPass Pass(const char* Name, hz::Span<hz::FrameGraphTextureUse> Uses, bool SideEffects = false)
    {
        return hz::AddFrameGraphPass(Graph, { .Name = Name, .Execute = Nothing, .Textures = Uses, .SideEffects = SideEffects });
    }
    bool Live(hz::FrameGraphPass Pass) const { return hz::IsFrameGraphPassLive(Graph, Pass); }
    uint32_t Physical(hz::FrameGraphTexture Texture) const { return Graph->Textures[Texture.Index].Physical; }

    hz::FrameGraph* Graph = nullptr;
};

TEST_F(FrameGraphTest, CullsPassesWhoseResultsNothingNeeds)
{
    const hz::FrameGraphTexture Output = hz::ImportFrameGraphTexture(Graph, { .Name = "Backbuffer", .Output = true });
    const hz::FrameGraphTexture Color = Texture();
    const hz::FrameGraphTexture Unused = Texture();
    const hz::FrameGraphTexture Overwritten = Texture();
    const hz::FrameGraphPass    Lost = Pass("Lost", { { Overwritten, hz::FrameGraphAccessWrite } });
    const hz::FrameGraphPass    Scene = hz::AddFrameGraphPass(Graph, { .Name = "Scene", .Execute = Nothing },
                                                              { { hz::FrameGraphAccessWrite, { Color, Overwritten } } });
    const hz::FrameGraphPass    Debug = Pass("Debug", { { Unused, hz::FrameGraphAccessWrite } });
    const hz::FrameGraphPass    Stats = Pass("Stats", { { Unused, hz::FrameGraphAccessWrite } }, true);
    const hz::FrameGraphPass    Overlay = Pass("Overlay", { { Color, hz::FrameGraphAccessModify }, { Overwritten, hz::FrameGraphAccessRead } });
    const hz::FrameGraphPass    Present = Pass("Present", { { Color, hz::FrameGraphAccessRead }, { Output, hz::FrameGraphAccessWrite } });
    ASSERT_TRUE(hz::ResizeFrameGraph(Graph, 64, 32));
    EXPECT_FALSE(Live(Lost));
    EXPECT_TRUE(Live(Scene));
    EXPECT_FALSE(Live(Debug));
    EXPECT_TRUE(Live(Stats));
    EXPECT_TRUE(Live(Overlay));
    EXPECT_TRUE(Live(Present));
}

TEST_F(FrameGraphTest, DisabledProducersCullTheirChainAndResolveOptionalReadsToNull)
{
    const hz::FrameGraphTexture Output = hz::ImportFrameGraphTexture(Graph, { .Name = "Backbuffer", .Output = true });
    const hz::FrameGraphTexture Occlusion = Texture(hz::Format::R8_UNORM);
    const hz::FrameGraphTexture Blur = Texture(hz::Format::R8_UNORM);
    const hz::FrameGraphPass    Trace = Pass("AO", { { Occlusion, hz::FrameGraphAccessWrite } });
    const hz::FrameGraphPass    Filter = Pass("Blur", { { Occlusion, hz::FrameGraphAccessRead }, { Blur, hz::FrameGraphAccessWrite } });
    const hz::FrameGraphPass    Light = hz::AddFrameGraphPass(Graph, { .Name = "Light", .Execute = Nothing },
                                                              { { hz::FrameGraphAccessReadOptional, { Blur } },
                                                                { hz::FrameGraphAccessWrite, { Output } } });
    ASSERT_TRUE(hz::ResizeFrameGraph(Graph, 64, 64));
    EXPECT_TRUE(Live(Trace) && Live(Filter) && Live(Light));

    hz::SetFrameGraphPassEnabled(Graph, Filter, false);
    ASSERT_TRUE(hz::CompileFrameGraph(Graph));
    EXPECT_FALSE(Live(Trace));
    EXPECT_FALSE(Live(Filter));
    EXPECT_TRUE(Live(Light));
    EXPECT_EQ(Physical(Blur), UINT32_MAX);

    // A required read with no live writer is a graph error.
    hz::SetFrameGraphPassEnabled(Graph, Filter, true);
    hz::SetFrameGraphPassEnabled(Graph, Trace, false);
    EXPECT_FALSE(hz::CompileFrameGraph(Graph));
}

TEST_F(FrameGraphTest, TransientTexturesShareOnlyWithDisjointLifetimesAndMatchingDescs)
{
    const hz::FrameGraphTexture Output = hz::ImportFrameGraphTexture(Graph, { .Name = "Backbuffer", .Output = true });
    const hz::FrameGraphTexture A = Texture();
    const hz::FrameGraphTexture B = Texture();
    const hz::FrameGraphTexture C = Texture();
    const hz::FrameGraphTexture Half = Texture(hz::Format::R8G8B8A8_UNORM, 1);
    const hz::FrameGraphTexture History = Texture(hz::Format::R8G8B8A8_UNORM, 0, true);
    // Lifetimes in pass order: A [0,1], B [1,2], C [2,3], Half [2,3]. A and C can share; B overlaps both.
    Pass("P0", { { A, hz::FrameGraphAccessWrite }, { History, hz::FrameGraphAccessReadHistory } });
    Pass("P1", { { A, hz::FrameGraphAccessRead }, { B, hz::FrameGraphAccessWrite }, { History, hz::FrameGraphAccessWrite } });
    Pass("P2", { { B, hz::FrameGraphAccessRead }, { C, hz::FrameGraphAccessWrite }, { Half, hz::FrameGraphAccessWrite } });
    Pass("P3", { { C, hz::FrameGraphAccessRead }, { Half, hz::FrameGraphAccessRead }, { Output, hz::FrameGraphAccessWrite } });
    ASSERT_TRUE(hz::ResizeFrameGraph(Graph, 128, 64));
    EXPECT_EQ(Physical(A), Physical(C));
    EXPECT_NE(Physical(A), Physical(B));
    EXPECT_NE(Physical(Half), Physical(A));
    EXPECT_NE(Physical(Half), Physical(B));
    EXPECT_EQ(Physical(History), UINT32_MAX);
    EXPECT_EQ(Graph->Physical.Size(), 3u);
    EXPECT_EQ(Graph->Textures[Half.Index].Resolved.Width, 64u);
    EXPECT_EQ(Graph->Textures[Half.Index].Resolved.Height, 32u);
}

TEST_F(FrameGraphTest, ResizeResolvesViewportSizesAndInvalidatesHistory)
{
    const hz::FrameGraphTexture Output = hz::ImportFrameGraphTexture(Graph, { .Name = "Backbuffer", .Output = true });
    const hz::FrameGraphTexture Tiny = Texture(hz::Format::R16_SFLOAT, 3);
    const hz::FrameGraphTexture Fixed = hz::AddFrameGraphTexture(Graph, { .Texture = { .Width = 16, .Height = 8, .Format = hz::Format::R8_UNORM } });
    const hz::FrameGraphTexture History = Texture(hz::Format::R16_SFLOAT, 0, true);
    const hz::FrameGraphBuffer  Nodes = hz::AddFrameGraphBuffer(Graph, { .Buffer = { .Name = "Nodes" }, .BytesPerPixel = 16 });
    const hz::FrameGraphBuffer Tiles = hz::AddFrameGraphBuffer(Graph, { .BytesPerTile = 32, .TileSize = 2 });
    hz::FrameGraphBufferUse     NodeUse = { Nodes, hz::FrameGraphAccessWrite };
    const hz::FrameGraphPass    Temporal =
        hz::AddFrameGraphPass(Graph, { .Name = "Temporal",
                                       .Execute = Nothing,
                                       .Textures = { { History, hz::FrameGraphAccessReadHistory }, { History, hz::FrameGraphAccessWrite } },
                                       .SideEffects = true });
    hz::FrameGraphTextureUse    Uses[] = { { Tiny, hz::FrameGraphAccessWrite }, { Fixed, hz::FrameGraphAccessWrite }, { Output, hz::FrameGraphAccessWrite } };
    hz::AddFrameGraphPass(Graph, { .Name = "Draw", .Execute = Nothing, .Textures = Uses, .Buffers = { &NodeUse, 1 } });
    ASSERT_TRUE(hz::ResizeFrameGraph(Graph, 4, 3));
    EXPECT_EQ(Graph->Textures[Tiny.Index].Resolved.Width, 1u);
    EXPECT_EQ(Graph->Textures[Tiny.Index].Resolved.Height, 1u);
    EXPECT_EQ(Graph->Textures[Fixed.Index].Resolved.Width, 16u);
    EXPECT_EQ(Graph->Buffers[Nodes.Index].ResolvedSize, 4u * 3u * 16u);

    EXPECT_EQ(Graph->Buffers[Tiles.Index].ResolvedSize, 2u * 2u * 32u);

    // History becomes valid after a frame in which its writer ran, and a resize or explicit reset invalidates it.
    EXPECT_FALSE(Graph->Textures[History.Index].HistoryValid);
    hz::FinishFrameGraphFrame(Graph);
    EXPECT_TRUE(Graph->Textures[History.Index].HistoryValid);
    hz::InvalidateFrameGraphHistory(Graph, History);
    EXPECT_FALSE(Graph->Textures[History.Index].HistoryValid);
    hz::FinishFrameGraphFrame(Graph);
    ASSERT_TRUE(hz::ResizeFrameGraph(Graph, 64, 64));
    EXPECT_FALSE(Graph->Textures[History.Index].HistoryValid);
    EXPECT_EQ(Graph->Textures[Tiny.Index].Resolved.Width, 8u);
    EXPECT_EQ(Graph->Buffers[Tiles.Index].ResolvedSize, 32u * 32u * 32u);

    // A frame whose writer did not run leaves no valid history behind.
    hz::SetFrameGraphPassEnabled(Graph, Temporal, false);
    ASSERT_TRUE(hz::CompileFrameGraph(Graph));
    hz::FinishFrameGraphFrame(Graph);
    EXPECT_FALSE(Graph->Textures[History.Index].HistoryValid);
}
