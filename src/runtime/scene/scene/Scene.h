#pragma once

#include <unordered_map>
#include <vector>

#include <runtime/function/rhi/RenderContext.h>
#include <runtime/function/rhi/vulkan/CommandBuffer.h>
#include <runtime/function/rhi/vulkan/Descriptors.h>
#include <runtime/function/rhi/vulkan/Device.h>
#include <runtime/scene/camera/Camera.h>
#include <runtime/scene/light/Light.h>
#include <runtime/scene/model/Model.h>

namespace Horizon {

#define MAX_LIGHT_COUNT 1024

class Scene {
  public:
    Scene(RenderContext &render_context, const std::shared_ptr<Device> &device,
          const std::shared_ptr<CommandBuffer> &command_buffer) noexcept;
    ~Scene() noexcept = default;

    void LoadModel(const std::string &path, const std::string &name) noexcept;
    std::shared_ptr<Model> GetModel(const std::string &name) const noexcept;

    // https://google.github.io/filament/Filament.html
    void AddDirectLight(Math::vec3 color, f32 intensity, Math::vec3 direction) noexcept;
    void AddPointLight(Math::vec3 color, f32 intensity, Math::vec3 position, f32 radius) noexcept;
    void AddSpotLight(Math::vec3 color, f32 intensity, Math::vec3 direction, Math::vec3 position, f32 radius,
                      f32 innerConeAngle, f32 outerConeAngle) noexcept;

    void Prepare() noexcept;
    void Draw(u32 i, std::shared_ptr<CommandBuffer> command_buffer, std::shared_ptr<Pipeline> pipeline) noexcept;
    std::shared_ptr<DescriptorSetLayouts> GetDescriptorLayouts() const noexcept;
    std::shared_ptr<DescriptorSetLayouts> GetGeometryPassDescriptorLayouts() const noexcept;
    std::shared_ptr<DescriptorSetLayouts> GetSceneDescriptorLayouts() const noexcept;
    std::shared_ptr<Camera> GetMainCamera() const noexcept;
    std::shared_ptr<UniformBuffer> getCameraUbo() const noexcept;

    std::shared_ptr<UniformBuffer> m_light_count_ub;
    std::shared_ptr<UniformBuffer> m_light_ub;
    std::shared_ptr<UniformBuffer> m_camera_ub;

  private:
    RenderContext &m_render_context;
    std::shared_ptr<Camera> m_camera = nullptr;
    std::shared_ptr<Device> m_device;
    std::shared_ptr<CommandBuffer> m_command_buffer;
    std::shared_ptr<DescriptorSet> m_scene_descriptor_set = nullptr;

    // uniform buffers

    // 0
    struct SceneUb {
        Math::mat4 view;
        Math::mat4 projection;
        Math::vec2 nearFar;
    } m_scene_ubdata;
    std::shared_ptr<UniformBuffer> m_scene_ub = nullptr;
    // 1
    struct LightCountUb {
        u32 lightCount = 0;
    } m_light_count_ubdata;

    // 2
    struct LightsUb {
        LightParams lights[MAX_LIGHT_COUNT];
    } m_lights_ubdata;

    // 3
    struct CamaeraUb {
        Math::vec3 camera_pos;
        f32 pad0;
        Math::vec3 camera_forward_dir;
        f32 pad1;
    } m_camera_ubdata;

    // models
    //std::vector<std::shared_ptr<Model>> m_models;
    std::unordered_map<std::string, std::shared_ptr<Model>> m_models;
};

class FullscreenTriangle {
  public:
    FullscreenTriangle(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer) noexcept;
    void Draw(u32 _i, std::shared_ptr<CommandBuffer> _command_buffer, std::shared_ptr<Pipeline> _pipeline,
              const std::vector<std::shared_ptr<DescriptorSet>> _descriptor_sets, bool _is_present = false) noexcept;

  private:
    std::shared_ptr<Device> m_device = nullptr;
    std::shared_ptr<CommandBuffer> m_command_buffer = nullptr;
    std::shared_ptr<VertexBuffer> m_vertex_buffer = nullptr;
    std::vector<Vertex> m_vertices;
};

//enum PrimitiveType {
//	Triangle
//};

//class Primitive {
//public:
//	Primitive(PrimitiveType );
//	void drawPrimitive(std::shared_ptr<Pipeline> pipeline);

//	std::shared_ptr<VertexBuffer> m_vertex_buffer = nullptr;
//	std::shared_ptr<IndexBuffer> m_index_buffer = nullptr;

//	std::vector<Vertex> vertices;
//	std::vector<u32> indices;
//};

} // namespace Horizon