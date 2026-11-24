#include "Editor/ViewportPicking.h"
#include "Editor/SceneInspector.h"
#include "Core/IFileSystem.h"
#include <string.h>

static constexpr ResourceDirectory EditorShaders = DirectoryMiddleware0;

ViewportPicking::ViewportPicking(hz::RenderContext& Context, const ViewportGeometry& Geometry): Context(Context), Geometry(Geometry)
{
    SetResourceDirectoryPath(SystemFileIO, MountProject, EditorShaders, "Source/Editor/Shaders");
}

void ViewportPicking::PreparePicking()
{
    if (PickPipeline.IsValid())
    {
        return;
    }
    PickPipeline = Context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
                                                                                { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PickPS" } },
                                                                    .pFileName = Geometry.Shader },
                                                    .Depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
                                                    .colorTargets = { { .Format = hz::Format::R32G32_UINT } },
                                                    .depthStencilFormat = Geometry.DepthFormat,
                                                    .indirectDrawId = true,
                                                    .Name = "Renderer.Pick" });
    PickReadPipeline =
        Context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } },
                                                        .sourceDirectory = EditorShaders,
                                                        .pFileName = "ViewportPickReadback.slang" },
                                        .Name = "Renderer.PickReadback" });
    PickIds = Context.CreateTexture({ .Width = 8,
                                      .Height = 8,
                                      .Format = hz::Format::R32G32_UINT,
                                      .StartState = RESOURCE_STATE_RENDER_TARGET,
                                      .Descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                      .GetRenderTarget = true,
                                      .Name = "Pick.InstanceId" });
    PickDepth = Context.CreateTexture({ .Width = 8,
                                        .Height = 8,
                                        .Format = Geometry.DepthFormat,
                                        .StartState = RESOURCE_STATE_DEPTH_WRITE,
                                        .GetRenderTarget = true,
                                        .Name = "Pick.Depth" });
    PickOutput = Context.CreateBuffer({ .Size = 8,
                                        .ElementCount = 1,
                                        .StructStride = 8,
                                        .Name = "Pick.Result",
                                        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                        .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                        .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    for (PickReadback& Readback : PickResults)
    {
        Readback.Buffer = Context.CreateBuffer({ .Size = 8,
                                                 .Name = "Pick.Readback",
                                                 .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                 .StartState = RESOURCE_STATE_COPY_DEST,
                                                 .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        ASSERT(Readback.Buffer.IsValid());
    }
    ASSERT(PickPipeline.IsValid() && PickReadPipeline.IsValid() && PickIds.IsValid() && PickDepth.IsValid() && PickOutput.IsValid());
}

void ViewportPicking::Cancel()
{
    PendingPick = 0;
    for (PickReadback& Result : PickResults)
    {
        Result.Serial = 0;
    }
}

void ViewportPicking::Update(SceneInspector& Inspector, const hz::RenderScene& Scene)
{
    const uint64_t Serial = Inspector.TakeViewportClick(Context.GetWidth(), Context.GetHeight(), PickX, PickY);
    if (Serial)
    {
        PendingPick = Serial;
    }
    for (PickReadback& Result : PickResults)
    {
        if (Result.Submission.IsValid() && Context.IsComplete(Result.Submission))
        {
            if (Result.Serial)
            {
                hz::RenderInstanceID Id;
                memcpy(&Id, Result.Buffer.Get()->pCpuMappedAddress, sizeof(Id));
                Inspector.ApplyPick(Result.Serial, Scene, Id);
            }
            Result.Submission = {};
        }
    }
}

bool ViewportPicking::HasWork(hz::RenderPrimitiveID Selected) const
{
    if (Selected.IsValid())
    {
        return true;
    }
    if (PendingPick)
    {
        for (const PickReadback& Result : PickResults)
        {
            if (!Result.Submission.IsValid())
            {
                return true;
            }
        }
    }
    return false;
}

void ViewportPicking::PrepareOutline(const hz::GPUTexture& Backbuffer)
{
    if (!MaskPipeline.IsValid())
    {
        MaskPipeline =
            Context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "MaskVS" },
                                                                         { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "MaskPS" } },
                                                             .pFileName = Geometry.Shader },
                                             .Depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
                                             .colorTargets = { { .Format = hz::Format::R8_UNORM } },
                                             .depthStencilFormat = Geometry.DepthFormat,
                                             .indirectDrawId = true,
                                             .Name = "Inspector.SelectionMask" });
        ASSERT(MaskPipeline.IsValid());
    }
    if (!OutlinePipeline.IsValid() || OutlineFormat != Backbuffer.GetFormat())
    {
        OutlineFormat = Backbuffer.GetFormat();
        OutlinePipeline = Context.CreateGraphicsPipeline(
            { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                          { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                              .sourceDirectory = EditorShaders,
                              .pFileName = "SelectionOutline.slang" },
              .colorTargets = { { .Format = OutlineFormat,
                                  .srcFactor = BC_SRC_ALPHA,
                                  .dstFactor = BC_ONE_MINUS_SRC_ALPHA,
                                  .colorWriteMask = ColorMask(COLOR_MASK_RED | COLOR_MASK_GREEN | COLOR_MASK_BLUE) } },
              .Name = "Editor.SelectionOutline" });
        ASSERT(OutlinePipeline.IsValid());
    }
    if (!OutlineSampler.IsValid())
    {
        OutlineSampler = Context.CreateSampler({ .AddressU = ADDRESS_MODE_CLAMP_TO_EDGE, .AddressV = ADDRESS_MODE_CLAMP_TO_EDGE });
    }
    if (!SelectionMask.IsValid() || SelectionMask.GetWidth() != Backbuffer.GetWidth() ||
        SelectionMask.GetHeight() != Backbuffer.GetHeight())
    {
        SelectionMask = Context.CreateTexture({ .Width = Backbuffer.GetWidth(),
                                                .Height = Backbuffer.GetHeight(),
                                                .Format = hz::Format::R8_UNORM,
                                                .StartState = RESOURCE_STATE_RENDER_TARGET,
                                                .Descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                                .GetRenderTarget = true,
                                                .Name = "Editor.SelectionMask" });
        SelectionDepth = Context.CreateTexture({ .Width = Backbuffer.GetWidth(),
                                                 .Height = Backbuffer.GetHeight(),
                                                 .Format = Geometry.DepthFormat,
                                                 .StartState = RESOURCE_STATE_DEPTH_WRITE,
                                                 .GetRenderTarget = true,
                                                 .Name = "Editor.SelectionDepth" });
        ASSERT(SelectionMask.IsValid() && SelectionDepth.IsValid());
    }
}

void ViewportPicking::DrawIds(hz::CommandList& Cmd, const hz::GPUBuffer& Frame, const hz::SceneView& View, const hz::GpuDrawList& Draws,
                              const hz::GPUSampler& Sampler, bool TransparentEnabled, hz::RenderPrimitiveID Selected,
                              const hz::GPUTexture& Target, const hz::GPUTexture& Depth, uint32_t OriginX, uint32_t OriginY)
{
    const hz::GPUBuffer* Lists[] = { &Draws.GetCommands(View.FrameIndex, Draws.GetOpaqueList()),
                                     &Draws.GetCommands(View.FrameIndex, Draws.GetTransparentList()) };
    const uint32_t       Count = TransparentEnabled ? 2u : 1u;
    hz::Dependencies     Dependencies = Draws.GetDependencies(Frame, View);
    Dependencies.indirectBuffers = { Lists, Count };
    Cmd.BeginRendering(
        {
            .colorAttachments = { { .pTexture = &Target, .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE } },
            .depthAttachment = { .pTexture = &Depth,
                                 .loadAction = LOAD_ACTION_CLEAR,
                                 .storeAction = STORE_ACTION_DONTCARE,
                                 .ClearValue = { .Depth = 1.0f } },
        },
        Dependencies);
    Cmd.SetViewport(-float(OriginX), -float(OriginY), float(Context.GetWidth()), float(Context.GetHeight()));
    Cmd.SetScissor(0, 0, Target.GetWidth(), Target.GetHeight());
    Cmd.SetPipeline(Selected.IsValid() ? MaskPipeline : PickPipeline);
    if (Draws.GetCount(View.FrameIndex))
    {
        const hz::GpuSceneBuffers& Gpu = View.GpuScene.GetBuffers();
        if (Selected.IsValid())
        {
            Cmd.SetRootConstant({ Selected.index, Selected.generation }, 1);
        }
        Cmd.SetRootConstant(
            { Frame.GetSrvIndex(), Gpu.instances.GetSrvIndex(), Gpu.materials.GetSrvIndex(), UINT32_MAX, Sampler.GetIndex() },
            Geometry.BindingSlot);
        Cmd.SetRootConstant({ Draws.GetDrawData(View.FrameIndex).GetSrvIndex() }, Geometry.DrawSlot);
        for (uint32_t I = 0; I < Count; ++I)
        {
            Cmd.DrawIndirect(*Lists[I], 0, Draws.GetCount(View.FrameIndex));
        }
    }
    Cmd.EndRendering();
}

void ViewportPicking::Record(hz::CommandList& Cmd, const hz::GPUBuffer& Frame, const hz::SceneView& View, const hz::GpuDrawList& Draws,
                             const hz::GPUSampler& Sampler, bool TransparentEnabled, hz::RenderPrimitiveID Selected,
                             const hz::GPUTexture& Backbuffer, float JitterX, float JitterY)
{
    if (Selected.IsValid())
    {
        PrepareOutline(Backbuffer);
    }
    PickReadback* Readback = nullptr;
    if (PendingPick)
    {
        for (PickReadback& Entry : PickResults)
        {
            if (!Entry.Submission.IsValid())
            {
                Readback = &Entry;
                break;
            }
        }
    }
    if (Readback)
    {
        PreparePicking();
        Readback->Serial = PendingPick;
        PendingPick = 0;
        DrawIds(Cmd, Frame, View, Draws, Sampler, TransparentEnabled, {}, PickIds, PickDepth, PickX & ~7u, PickY & ~7u);
        Cmd.SetPipeline(PickReadPipeline);
        Cmd.SetRootConstant({ PickIds.GetSrvIndex(), PickOutput.GetUavIndex(), PickX & 7u, PickY & 7u });
        Cmd.Dispatch(1, 1, 1, { .sampledTextures = { &PickIds }, .storageBuffers = { &PickOutput } });
        Cmd.CopyBuffer(Readback->Buffer, 0, PickOutput, 0, 8);
    }
    if (Selected.IsValid())
    {
        DrawIds(Cmd, Frame, View, Draws, Sampler, TransparentEnabled, Selected, SelectionMask, SelectionDepth, 0, 0);
        const struct
        {
            uint32_t Mask, Sampler;
            float    JitterX, JitterY;
            uint32_t Mode, EncodeSRGB;
        } Constants = { SelectionMask.GetSrvIndex(),
                        OutlineSampler.GetIndex(),
                        JitterX,
                        JitterY,
                        uint32_t(Context.GetOutputMode()),
                        OutlineFormat != hz::Format::R8G8B8A8_SRGB && OutlineFormat != hz::Format::B8G8R8A8_SRGB };
        Cmd.BeginRendering(
            { .colorAttachments = { { .pTexture = &Backbuffer, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } } },
            { .sampledTextures = { &SelectionMask } });
        Cmd.SetViewport(0, 0, float(Backbuffer.GetWidth()), float(Backbuffer.GetHeight()));
        Cmd.SetScissor(0, 0, Backbuffer.GetWidth(), Backbuffer.GetHeight());
        Cmd.SetPipeline(OutlinePipeline);
        Cmd.SetRootConstants(Constants);
        Cmd.Draw(3);
        Cmd.EndRendering();
    }
    RecordedPick = Readback;
}

void ViewportPicking::OnSubmitted(hz::SubmitHandle Submission)
{
    if (RecordedPick)
    {
        RecordedPick->Submission = Submission;
        RecordedPick = nullptr;
    }
}
