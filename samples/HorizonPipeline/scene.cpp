#include "scene.h"

#include <random>

std::filesystem::path asset_path = ASSET_DIR;

SceneData::SceneData(SceneManager *scene_manager, Backend::RHI *rhi) noexcept {
    m_scene_manager = scene_manager;

    scene_manager->CreateBuiltInResources(rhi);
    auto [camera, controller] = m_scene_manager->AddCamera(
        CameraSetting{ProjectionMode::PERSPECTIVE, CameraType::FLY, true}, Math::float3(0.0, 0.0, 10.0_m),
        Math::float3(0.0, 0.0, 0.0), Math::float3(0.0, 1.0_m, 0.0));

    scene_camera = camera;
    scene_camera_controller = controller;

    scene_camera->SetCameraSpeed(0.1);

    scene_camera->SetExposure(16.0f, 1 / 125.0f, 100.0f);

    scene_camera->SetPerspectiveProjectionMatrix(90.0_deg, (float)width / (float)height, 0.1f, 100.0f);
    // cam->SetLensProjectionMatrix(18.0, (float)width / (float)height, 0.1f, 100.0f);

    //auto sphere = scene_manager->AddMesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL |
    //                                VertexAttributeType::UV0 | VertexAttributeType::TANGENT},
    //                       asset_path / "models/Cauldron-Media/MetalRoughSpheres/glTF/MetalRoughSpheres.gltf");

    auto sponza =
        scene_manager->resource_manager->LoadMesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL |
                                                           VertexAttributeType::UV0 | VertexAttributeType::TANGENT},
                                                  asset_path / "models/Sponza/glTF/Sponza.gltf");

    //auto m = Math::float4x4::CreateTranslation(Math::float3(0.0, 20.0, 0.0));
    //sphere->transform = m;
    // auto sphere =
    //     scene_manager->resource_manager->LoadMesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL |
    //                                                        VertexAttributeType::UV0 | VertexAttributeType::TANGENT},
    //                                               asset_path / "models/MetalRoughSpheres/glTF/MetalRoughSpheres.gltf");
    scene_manager->AddMesh(sponza);

    //auto cube = scene_manager->resource_manager->LoadMesh(
    //    MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL | VertexAttributeType::UV0 |
    //             VertexAttributeType::TANGENT},
    //    "C:/Users/hylu/OneDrive/mycode/DredgenGraphicEngine/Dredgen-gl/resources/models/Cube/glTF/Cube.gltf");
    ////scene_manager->AddMesh(cube);
    //for (auto &v : cube->m_vertices) {
    //    LOG_INFO("Vertex(Math::float3({},{},{}),Math::float3({},{},{}), Math::float2({},{})),", v.pos.x, v.pos.y,
    //             v.pos.z, v.normal.x, v.normal.y, v.normal.z, v.uv0.x, v.uv0.y);
    //}
    //scene_manager->AddMesh();
    //auto helmet = scene_manager->AddMesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL |
    //                                VertexAttributeType::UV0 | VertexAttributeType::TANGENT},
    //                       asset_path / "models/Cauldron-Media/DamagedHelmet/glTF/DamagedHelmet.gltf");
    //m = Math::float4x4::CreateTranslation(Math::float3(5.0, 5.0, 0.0));
    //helmet->transform = m;

    scene_manager->CreateMeshResources(rhi);

    std::random_device seed;
    std::ranlux48 engine(seed());
    std::uniform_real_distribution<f32> random_position(-10.0, 10.0);
    std::uniform_real_distribution<f32> random_color(0.5, 1.0);

    for (unsigned int i = 0; i < 64; i++) {
        // calculate slightly random offsets
        float xPos = random_position(engine);
        float yPos = random_position(engine);
        float zPos = random_position(engine) + 2.0f;

        // also calculate random color
        float rColor = random_color(engine);
        float gColor = random_color(engine);
        float bColor = random_color(engine);

        Math::float3 pos(xPos, yPos, zPos);
        Math::float3 col(rColor, gColor, bColor);

        scene_manager->AddPointLight(col, 1000000.0_lm, pos, 10.0);
    }

    scene_manager->AddDirectionalLight(Math::float3(1.0, 1.0, 1.0), 120000.0_lux, Math::float3(0.0, 0.0, -1.0));

    scene_manager->CreateLightResources(rhi);
    scene_manager->CreateCameraResources(rhi);
}
