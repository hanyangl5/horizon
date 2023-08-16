#include "scene.h"

#include <random>

SceneData::SceneData(SceneManager *scene_manager) noexcept {
    m_scene_manager = scene_manager;

    scene_manager->CreateBuiltInResources();
    auto [camera, controller] = m_scene_manager->AddCamera(
        CameraSetting{ProjectionMode::PERSPECTIVE, CameraType::FLY, true}, Math::float3(0.0, 2.0, 0.0),
        Math::float3(5.0, 2.0, 0.0), Math::float3(0.0, 1.0, 0.0));

    scene_camera = camera;
    scene_camera_controller = controller;

    scene_camera->SetCameraSpeed(0.1f);

    scene_camera->SetExposure(16.0f, 1 / 125.0f, 100.0f);

    scene_camera->SetPerspectiveProjectionMatrix(Math::Radians(90.0f), (float)width / (float)height, 0.1f, 100.0f);

    auto sponza =
        scene_manager->resource_manager->LoadMesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL |
                                                           VertexAttributeType::UV0 | VertexAttributeType::TANGENT},
                                                  asset_path / "models/Sponza/glTF/Sponza.gltf");
    scene_manager->AddMesh(sponza);

    scene_manager->CreateMeshResources();

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

        // lumincance
        scene_manager->AddPointLight(col, 1000000.0f * Math::_1DIVPI * 0.25f, pos, 10.0f);
    }

    scene_manager->AddDirectionalLight(Math::float3(1.0, 1.0, 1.0), 120000.0f, Math::float3(0.0, 0.0, -1.0));

    scene_manager->CreateLightResources();
    scene_manager->CreateCameraResources();
}
