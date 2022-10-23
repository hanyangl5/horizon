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

    // mesh
    {
        auto mesh1 = new Mesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL |
                                       VertexAttributeType::UV0 | VertexAttributeType::TANGENT});
        // mesh1->LoadMesh(asset_path / "models/FlightHelmet/glTF/FlightHelmet.gltf");
        // mesh1->LoadMesh(asset_path / "models/DamagedHelmet/DamagedHelmet.gltf");
        mesh1->LoadMesh(asset_path / "models/Cauldron-Media/MetalRoughSpheres/glTF/MetalRoughSpheres.gltf");
        mesh1->CreateGpuResources(rhi);

        auto mesh2 = new Mesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL |
                                       VertexAttributeType::UV0 | VertexAttributeType::TANGENT});
        mesh2->LoadMesh(asset_path / "models/Sponza/glTF/Sponza.gltf");
        // mesh2->LoadMesh("C://Users//hylu//OneDrive//Program//Computer
        // Graphics//models//Main.1_Sponza//NewSponza_Main_glTF_002.gltf");
        // mesh2->LoadMesh("C:/Users/hylu/Downloads/Cauldron-Media-6e7b1a5608f5f18ff4e38541eec147bc9099a759/Cauldron-Media-6e7b1a5608f5f18ff4e38541eec147bc9099a759/MetalRoughSpheres/glTF/MetalRoughSpheres.gltf");
        // mesh2->LoadMesh(asset_path / "models/dragon/untitled.gltf");
        mesh2->CreateGpuResources(rhi);

        meshes.push_back(mesh1);
        meshes.push_back(mesh2);
    }
}
