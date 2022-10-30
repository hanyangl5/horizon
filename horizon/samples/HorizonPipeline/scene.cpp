#include "scene.h"

SceneData::SceneData(Backend::RHI *rhi, Camera *camera) noexcept {

    // camera
    {
        cam = camera;

        cam->SetPerspectiveProjectionMatrix(90.0_deg, (float)_width / (float)_height, 0.1f, 100.0f);
        // cam->SetLensProjectionMatrix(18.0, (float)width / (float)height, 0.1f, 100.0f);

        camera_buffer =
            rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                               ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(CameraUb)});
    }
    // light
    {
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

            // lights.push_back(new PointLight(col, 1000000.0_lm, pos, 10.0));
        }

        lights.push_back(new DirectionalLight(Math::float3(1.0, 1.0, 1.0), 120000.0_lux, Math::float3(0.0, 0.0, -1.0)));

        for (auto &l : lights) {
            lights_param_buffer.push_back(l->GetParamBuffer());
        }
        light_count = lights.size();

        light_count_buffer =
            rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                               ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 4 * sizeof(u32)});

        light_buffer = rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                                          ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                                                          sizeof(LightParams) * light_count});
    }

    
    scene_manager = std::make_unique<SceneManager>();

    //auto sphere = scene_manager->AddMesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL |
    //                                VertexAttributeType::UV0 | VertexAttributeType::TANGENT},
    //                       asset_path / "models/Cauldron-Media/MetalRoughSpheres/glTF/MetalRoughSpheres.gltf");

    //MeshLoader::Load(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL |
    //                                VertexAttributeType::UV0 | VertexAttributeType::TANGENT},
    //                       asset_path / "models/Sponza/glTF/Sponza.gltf");

    //auto m = Math::float4x4::CreateTranslation(Math::float3(0.0, 20.0, 0.0));
    //sphere->transform = m;
    scene_manager->AddMesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL |
                                    VertexAttributeType::UV0 | VertexAttributeType::TANGENT},
                           asset_path / "models/Sponza/glTF/Sponza.gltf");


    //auto helmet = scene_manager->AddMesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL |
    //                                VertexAttributeType::UV0 | VertexAttributeType::TANGENT},
    //                       asset_path / "models/Cauldron-Media/DamagedHelmet/glTF/DamagedHelmet.gltf");
    //m = Math::float4x4::CreateTranslation(Math::float3(5.0, 5.0, 0.0));
    //helmet->transform = m;


    scene_manager->CreateMeshResources(rhi);
}
