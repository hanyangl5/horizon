#include "shadow.h"

ConatactShadowData::ConatactShadowData(RHI *rhi) noexcept {}

ConatactShadowData::~ConatactShadowData() noexcept {}

ShadowMapData::ShadowMapData(RHI *rhi) noexcept {
    shadow_map_vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, asset_path / "shaders/decal.vert.hsl");
    shadow_map_ps = rhi->CreateShader(ShaderType::PIXEL_SHADER, 0, asset_path / "shaders/decal.frag.hsl");

    GraphicsPipelineCreateInfo graphics_pass_ci{};
    graphics_pass_ci.vertex_input_state.attribute_count = 5;

    auto &pos = graphics_pass_ci.vertex_input_state.attributes[0];
    pos.attrib_format = VertexAttribFormat::F32; // position
    pos.portion = 3;
    pos.binding = 0;
    pos.location = 0;
    pos.offset = 0;
    pos.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;

    auto &normal = graphics_pass_ci.vertex_input_state.attributes[1];
    normal.attrib_format = VertexAttribFormat::F32; // normal, TOOD: SN16 is a better format
    normal.portion = 3;
    normal.binding = 0;
    normal.location = 1;
    normal.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
    normal.offset = offsetof(Vertex, normal);

    auto &uv0 = graphics_pass_ci.vertex_input_state.attributes[2];
    uv0.attrib_format = VertexAttribFormat::F32; // uv0 TOOD: UN16 is a better format
    uv0.portion = 2;
    uv0.binding = 0;
    uv0.location = 2;
    uv0.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
    uv0.offset = offsetof(Vertex, uv0);

    auto &uv1 = graphics_pass_ci.vertex_input_state.attributes[3];
    uv1.attrib_format = VertexAttribFormat::F32; // uv1 TOOD: UN16 is a better format
    uv1.portion = 2;
    uv1.binding = 0;
    uv1.location = 3;
    uv1.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
    uv1.offset = offsetof(Vertex, uv1);

    auto &tangent = graphics_pass_ci.vertex_input_state.attributes[4];
    tangent.attrib_format = VertexAttribFormat::F32;
    tangent.portion = 3;
    tangent.binding = 0;
    tangent.location = 4;
    tangent.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
    tangent.offset = offsetof(Vertex, tangent);

    graphics_pass_ci.view_port_state.width = _width;
    graphics_pass_ci.view_port_state.height = _height;

    graphics_pass_ci.depth_stencil_state.depth_func = DepthFunc::LESS;
    graphics_pass_ci.depth_stencil_state.depthNear = 0.0f;
    graphics_pass_ci.depth_stencil_state.depthNear = 1.0f;
    graphics_pass_ci.depth_stencil_state.depth_test = true;
    graphics_pass_ci.depth_stencil_state.depth_write = false;
    graphics_pass_ci.depth_stencil_state.stencil_enabled = false;

    graphics_pass_ci.input_assembly_state.topology = PrimitiveTopology::TRIANGLE_LIST;

    graphics_pass_ci.multi_sample_state.sample_count = 1;

    graphics_pass_ci.rasterization_state.cull_mode = CullMode::BACK;
    graphics_pass_ci.rasterization_state.discard = false;
    graphics_pass_ci.rasterization_state.fill_mode = FillMode::TRIANGLE;
    graphics_pass_ci.rasterization_state.front_face = FrontFace::CCW;

    graphics_pass_ci.render_target_formats.color_attachment_count = 0;

    graphics_pass_ci.render_target_formats.has_depth = true;
    graphics_pass_ci.render_target_formats.depth_stencil_format = TextureFormat::TEXTURE_FORMAT_D32_SFLOAT;

    shadow_map_pass = rhi->CreateGraphicsPipeline(graphics_pass_ci);
    shadow_map_pass->SetGraphicsShader(shadow_map_vs, shadow_map_ps);

}
ShadowMapData::~ShadowMapData() noexcept {}
