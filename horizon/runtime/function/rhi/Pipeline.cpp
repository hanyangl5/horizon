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
            LOG_ERROR("invalid frequency");
        }

        for (auto &descriptor : frequency.value()) {
            auto name = descriptor["name"].get<std::string>();
            desc.type = static_cast<DescriptorType>(descriptor["type"].get<u32>());
            desc.vk_binding = descriptor["vk_binding"];
            // auto reg = descriptor["dx_register"]; TODO
            rsd._descs[static_cast<u32>(freq)].try_emplace(name, desc);

            vk_binding_count[static_cast<u32>(freq)] =
                std::max(vk_binding_count[static_cast<u32>(freq)], desc.vk_binding + 1);
        }
    }
}

} // namespace Horizon::RHI
