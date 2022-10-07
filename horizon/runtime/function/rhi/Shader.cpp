#include "Shader.h"

#include <utility>

#include <nlohmann/json.hpp>

namespace Horizon::RHI {

Shader::Shader(ShaderType type, const std::vector<char> &rsd_code) noexcept : m_type(type) {
    
    nlohmann::json data = nlohmann::json::parse(rsd_code);
    DescriptorSetDesc desc{};
    for (auto &frequency : data) {
        if (frequency.type_name() == "UPDATE_FREQ_NONE") {
            desc.update_frequency = ResourceUpdateFrequency::NONE;
        } else if (frequency.type_name() == "UPDATE_FREQ_PER_FRAME") {
            desc.update_frequency = ResourceUpdateFrequency::PER_FRAME;
        } else if (frequency.type_name() == "UPDATE_FREQ_PER_BATCH") {
            desc.update_frequency = ResourceUpdateFrequency::PER_BATCH;
        } else if (frequency.type_name() == "UPDATE_FREQ_PER_DRAW") {
            desc.update_frequency = ResourceUpdateFrequency::PER_DRAW;
        } else {
            LOG_ERROR("invalid frequency");
        }

        for (auto &descriptor : frequency) {
            desc.name = descriptor["name"].get<std::string>();
            auto type = descriptor["type"];
            auto binding = descriptor["vk_binding"];
            auto reg = descriptor["dx_register"];
            rsd.descs.push_back(desc);
        }
    }
    
}

ShaderType Shader::GetType() const noexcept { return m_type; }

const std::string &Shader::GetEntryPoint() const noexcept { return m_entry_point; }
} // namespace Horizon::RHI