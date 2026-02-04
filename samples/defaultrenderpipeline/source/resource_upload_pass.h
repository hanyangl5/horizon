#pragma once

#include "header.h"
#include <render_graph/frame_graph.h>

class DeferredShadingPass;
class AmbientOcclusionPass;
class PostProcessingPass;
class AntialiasingPass;

class ResourceUploadPass
{
  public:
    ResourceUploadPass() = default;
    ~ResourceUploadPass() = default;

    void Setup(Horizon::Backend::FrameGraphBuilder &builder, Horizon::Backend::TextureHandle shading_color_handle,
               Horizon::Backend::TextureHandle pp_color_handle, Horizon::Backend::TextureHandle ssao_factor_handle,
               Horizon::Backend::TextureHandle ssao_blur_handle, Horizon::Backend::TextureHandle output_color_handle,
               Horizon::Backend::TextureHandle previous_color_handle, Horizon::Backend::TextureHandle ssao_noise_handle,
               Horizon::Backend::TextureHandle brdf_lut_handle, Horizon::Backend::TextureHandle prefiltered_env_handle,
               Horizon::Backend::BufferHandle histogram_buffer_handle,
               Horizon::Backend::BufferHandle adapted_luminance_handle, bool first_frame);

    void Execute(CommandList *cl, Horizon::SceneManager *scene_manager, DeferredShadingPass *deferred,
                 AmbientOcclusionPass *ssao, PostProcessingPass *post_process, AntialiasingPass *antialiasing,
                 bool first_frame);
};
