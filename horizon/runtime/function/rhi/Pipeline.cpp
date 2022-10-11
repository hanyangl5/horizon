#include "Pipeline.h"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace Horizon::RHI {

Pipeline::Pipeline() noexcept {}

Pipeline::~Pipeline() noexcept {}

PipelineType Pipeline::GetType() const noexcept { return m_create_info.type; }

void Pipeline::ParseRootSignature() {
    if (m_create_info.type == PipelineType::GRAPHICS) {
        ParseRootSignatureFromShader(m_vs);
        ParseRootSignatureFromShader(m_ps);
    } else if (m_create_info.type == PipelineType::COMPUTE) {
        ParseRootSignatureFromShader(m_cs);
    }
}

void Pipeline::ParseRootSignatureFromShader(Shader *shader) {
    auto &path = shader->GetRootSignatureDescriptionPath();
    auto rsd_code = ReadFile(path.generic_string().c_str());
    nlohmann::json json_data = nlohmann::json::parse(rsd_code);

    DescriptorDesc desc{};

    for (auto &frequency : json_data.items()) {
        ResourceUpdateFrequency freq{};
        if (frequency.key() == "UPDATE_FREQ_NONE") {
            freq = ResourceUpdateFrequency::NONE;
        } else if (frequency.key() == "UPDATE_FREQ_PER_FRAME") {
            freq = ResourceUpdateFrequency::PER_FRAME;
        } else if (frequency.key() == "UPDATE_FREQ_PER_BATCH") {
            freq = ResourceUpdateFrequency::PER_BATCH;
        } else if (frequency.key() == "UPDATE_FREQ_PER_DRAW") {
            freq = ResourceUpdateFrequency::PER_DRAW;
        } else {
            continue;
        }

        for (auto &descriptor : frequency.value()) {
            auto name = descriptor["name"].get<std::string>();
            desc.type = static_cast<DescriptorType>(descriptor["descriptor_type"].get<u32>());
            desc.vk_binding = descriptor["vk_binding"].get<u32>();
            // auto reg = descriptor["dx_register"]; TODO
            rsd.descriptors[static_cast<u32>(freq)].try_emplace(name, desc);

            vk_binding_count[static_cast<u32>(freq)] =
                std::max(vk_binding_count[static_cast<u32>(freq)], desc.vk_binding + 1);
        }
    }

    auto &pc_descs = json_data["PUSH_CONSTANT"];
    if (!pc_descs.empty()) {
        for (auto &pc_desc : pc_descs) {
            auto pc_name = pc_desc["name"].get<std::string>();
            auto &dst = rsd.push_constants[pc_name];
            dst.size = pc_desc["size"].get<u32>();
            dst.offset = 0;
            dst.shader_stages |= pc_desc["stage"].get<u32>();
        }
    }
}

} // namespace Horizon::RHI
