//#pragma once
//
//#include <string>
//#include <unordered_map>
//#include <vector>
//
//#include <runtime/core/math/Math.h>
//#include <runtime/core/utils/definations.h>
//#include <runtime/function/scene/camera/Camera.h>
//#include <runtime/function/scene/geometry/mesh/Mesh.h>
//#include <runtime/function/scene/light/Light.h>
//
//namespace Horizon {
//
//class SceneManager {
//  public:
//    SceneManager() noexcept;
//    ~SceneManager() noexcept;
//
//    Mesh *AddMesh(const MeshDesc &desc, const std::string &path) noexcept;
//    Mesh *AddMesh(BasicGeometry basic_geometry) noexcept;
//    Light *AddDirectionalLight(
//        Math::color color, f32 intensity,
//        Math::float3
//            directiona) noexcept; // temperature, soource radius, length
//    Light *AddPointLight(Math::float3 color, f32 intensity,
//                         f32 radius) noexcept;
//    Light *AddSpotLight(Math::float3 color, f32 intensity, f32 inner_cone,
//                        f32 outer_cone) noexcept;
//    Camera *SetMainCamera() noexcept;
//
//  private:
//    std::vector<std::unique_ptr<Mesh>> meshes;
//    std::vector<std::unique_ptr<Light>> lights;
//};
//
//} // namespace Horizon