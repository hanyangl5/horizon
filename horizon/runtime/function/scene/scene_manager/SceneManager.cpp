#include "SceneManager.h"

namespace Horizon {

SceneManager::SceneManager() {}

SceneManager::~SceneManager() {}

Mesh *SceneManager::AddMesh(const MeshDesc &desc, const std::string &path) {
    auto &m = meshes.emplace_back(std::make_unique<Mesh>(desc));
    m->LoadMesh(path);
    return m.get();
}

Mesh *SceneManager::AddMesh(BasicGeometry basic_geometry) {
    auto &m = meshes.emplace_back();
    m->LoadMesh(basic_geometry);
    return m.get();
}

Light *SceneManager::AddDirectionalLight(Math::color color, f32 intensity,
                                         Math::float3 direction) {
    auto &l = lights.emplace_back(
        std::make_unique<DirectionalLight>(color, intensity, direction));
    return l.get();
}

Light *SceneManager::AddPointLight(Math::float3 color, f32 intensity,
                                   f32 radius) {
    auto &l = lights.emplace_back(
        std::make_unique<PointLight>(color, intensity, radius));
    return l.get();
}

Light *SceneManager::AddSpotLight(Math::float3 color, f32 intensity,
                                  f32 inner_cone, f32 outer_cone) {
    auto &l = lights.emplace_back(
        std::make_unique<SpotLight>(color, intensity, inner_cone, outer_cone));
    return l.get();
}
} // namespace Horizon