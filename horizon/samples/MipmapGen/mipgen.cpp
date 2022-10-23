#include "mipgen.h"
#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
void MipmapGen::InitAPI() { rhi = engine->m_render_system->GetRhi(); }

void MipmapGen::InitResources() {

    InitPipelineResources();

    InitSceneResources();
}

void MipmapGen::InitPipelineResources() {

    // shaders
    { mipgen_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, 0, asset_path / "shaders/generate_mipmap.comp.hsl"); }

    {

        // AO PASS
        { mipgen_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{}); }

        // SamplerDesc sampler_desc{};
        // sampler_desc.min_filter = FilterType::FILTER_LINEAR;
        // sampler_desc.mag_filter = FilterType::FILTER_LINEAR;
        // sampler_desc.mip_map_mode = MipMapMode::MIPMAP_MODE_LINEAR;
        // sampler_desc.address_u = AddressMode::ADDRESS_MODE_REPEAT;
        // sampler_desc.address_v = AddressMode::ADDRESS_MODE_REPEAT;
        // sampler_desc.address_w = AddressMode::ADDRESS_MODE_REPEAT;

        // sampler = rhi->GetSampler(sampler_desc);
    }
}

void MipmapGen::InitSceneResources() {
    // scene resources
    {
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/715093869573992647.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/8773302468495022225.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/5061699253647017043.png", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/11872827283454512094.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/12501374198249454378.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/8006627369776289000.png", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/715093869573992647.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/4477655471536070370.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/7268504077753552595.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/8503262930880235456.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/13982482287905699490.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/8750083169368950601.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/16885566240357350108.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/16299174074766089871.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/5792855332885324923.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/11968150294050148237.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/2051777328469649772.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/14650633544276105767.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/4871783166746854860.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/10388182081421875623.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/15295713303328085182.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/9916269861720640319.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/15722799267630235092.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/6047387724914829168.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/8051790464816141987.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/14267839433702832875.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/5823059166183034438.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/13824894030729245199.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/6667038893015345571.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/7441062115984513793.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/8114461559286000061.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/3628158980083700836.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/11490520546946913238.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/3455394979645218238.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/7645212358685992005.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/6151467286084645207.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/8783994986360286082.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/2299742237651021498.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/4975155472559461469.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/3371964815757888145.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/7056944414013900257.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/4675343432951571524.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/7815564343179553343.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/2374361008830720677.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/2775690330959970771.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/332936164838540657.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/2185409758123873465.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/6593109234861095314.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/17876391417123941155.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/466164707995436622.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/4601176305987539675.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/11474523244911310074.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/4910669866631290573.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/9288698199695299068.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/14170708867020035030.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/16275776544635328252.png", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/1219024358953944284.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/3827035219084910048.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/755318871556304029.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/2411100444841994089.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/10381718147657362067.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/8481240838833932244.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/17556969131407844942.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/759203620573749278.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/6772804448157695701.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/13196865903111448057.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/14118779221266351425.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/2969916736137545357.jpg", nullptr});
        texture_resources.push_back(
            TextureData{"C:/FILES/horizon/horizon/assets/models/Sponza/glTF/8747919177698443163.jpg", nullptr});
    }

    for (auto &r : texture_resources) {
        int width, height, channels;
        u8 *data = stbi_load(r.path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
        assert(("failed to load texture", data != nullptr));

        if (width != 1024 || height != 1024) {
            LOG_ERROR("{}", r.path.string().c_str());
        }

        TextureCreateInfo create_info{};
        create_info.width = width;
        create_info.height = height;
        create_info.depth = 1;
        create_info.texture_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM; // TOOD: optimize format
        create_info.texture_type = TextureType::TEXTURE_TYPE_2D;                // TODO: cubemap?
        create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE;
        create_info.initial_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
        create_info.enanble_mipmap = true;
        r.texture = (rhi->CreateTexture(create_info));

        r.width = width;
        r.height = height;
        r.data = data;
    }
}

void MipmapGen::run() {

    bool first_frame = true;

    mipgen_pass->SetComputeShader(mipgen_cs);

    Horizon::RDC::StartFrameCapture();

    // upload resources
    auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);
    transfer->BeginRecording();

    // async
    for (u32 i = 0; i < texture_resources.size(); i++) {
        TextureUpdateDesc desc{};
        desc.data = texture_resources[i].data;
        // desc.data = ssao_noise_tex_val.data();
        transfer->UpdateTexture(texture_resources[i].texture.get(), desc);
        //
    }
    //{
    //    BarrierDesc barrier{};
    //    for (auto &r : texture_resources) {

    //        TextureBarrierDesc tb1;
    //        tb1.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
    //        tb1.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
    //        tb1.texture = r.texture.get();
    //        tb1.first_mip_level = 0;
    //        tb1.mip_level_count = r.texture->mip_map_level;
    //        barrier.texture_memory_barriers.push_back(tb1);
    //    }
    //    transfer->InsertBarrier(barrier);
    //}

    transfer->EndRecording();

    auto resource_uploaded_semaphore = rhi->GetSemaphore();
    {
        QueueSubmitInfo submit_info{};
        submit_info.command_lists.push_back(transfer);
        submit_info.queue_type = CommandQueueType::TRANSFER;
        submit_info.signal_semaphores.push_back(resource_uploaded_semaphore.get());
        rhi->SubmitCommandLists(submit_info);
    }

    auto mipmapgen_semaphore = rhi->GetSemaphore();
    {

        auto mipmap = rhi->GetCommandList(CommandQueueType::GRAPHICS);
        mipmap->BeginRecording();




        for (auto &r : texture_resources) {

            {
                BarrierDesc desc{};
                TextureBarrierDesc mip_map_barrier{};
                mip_map_barrier.texture = r.texture.get();
                mip_map_barrier.first_mip_level = 0;
                mip_map_barrier.mip_level_count = 1;
                mip_map_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                mip_map_barrier.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                desc.texture_memory_barriers.emplace_back(mip_map_barrier);
                mipmap->InsertBarrier(desc);
            }
            mipmap->GenerateMipMap(r.texture.get(), true);

            {
                BarrierDesc desc{};
                TextureBarrierDesc mip_map_barrier{};
                mip_map_barrier.texture = r.texture.get();
                mip_map_barrier.first_mip_level = 0;
                mip_map_barrier.mip_level_count = r.texture->mip_map_level;
                mip_map_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                mip_map_barrier.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                desc.texture_memory_barriers.emplace_back(mip_map_barrier);
                mipmap->InsertBarrier(desc);
            }
        }

       

        mipmap->EndRecording();
        {
            QueueSubmitInfo submit_info{};
            submit_info.command_lists.push_back(mipmap);
            submit_info.queue_type = CommandQueueType::GRAPHICS;
            submit_info.wait_semaphores.push_back(resource_uploaded_semaphore.get());
            submit_info.signal_semaphores.push_back(mipmapgen_semaphore.get());
            rhi->SubmitCommandLists(submit_info);
        }
    }

    // begin compute pipeline

    for (auto &r : texture_resources) {
        r.ds = mipgen_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_DRAW);

        r.ds->SetResource(r.texture.get(), "Source");
        r.ds->SetResource(r.texture.get(), "Destination");
        r.ds->Update();
    }

    auto compute = rhi->GetCommandList(CommandQueueType::COMPUTE);

    compute->BeginRecording();

    for (auto &r : texture_resources) {

        compute->BindPipeline(mipgen_pass);
        compute->BindDescriptorSets(mipgen_pass, r.ds);
        u32 mipSize[2] = {(u32)ceil(log2((float)r.width)), (u32)ceil(log2((float)r.width))};

        compute->BindPushConstant(mipgen_pass, "RootConstant", &mipSize);
        compute->Dispatch(r.width / 8 + 1, r.height / 8 + 1, 1);

        // for (u32 i = 1; i < pSSSR_DepthHierarchy->mMipLevels; ++i) {
        //     mipSizeX >>= 1;
        //     mipSizeY >>= 1;
        //     uint mipSize[2] = {mipSizeX, mipSizeY};
        //     cmdBindPushConstants(cmd, pGenerateMipRootSignature, gMipSizeRootConstantIndex, mipSize);
        //     cmdBindDescriptorSet(cmd, i - 1, pDescriptorGenerateMip);
    }
    compute->EndRecording();
    {
        QueueSubmitInfo submit_info{};
        submit_info.command_lists.push_back(compute);
        submit_info.queue_type = CommandQueueType::COMPUTE;
        submit_info.wait_semaphores.push_back(mipmapgen_semaphore.get());
        rhi->SubmitCommandLists(submit_info);
        rhi->WaitGpuExecution(CommandQueueType::COMPUTE);
    }
    Horizon::RDC::EndFrameCapture();

    for (auto &res : texture_resources) {
        stbi_image_free(res.data);
    }
    rhi->DestroyPipeline(mipgen_pass);
    rhi->DestroyShader(mipgen_cs);
}
