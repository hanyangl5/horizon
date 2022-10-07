#include "Shader.h"

#include <string>
#include <utility>
#include <algorithm>

#include <nlohmann/json.hpp>


namespace Horizon::RHI {

Shader::Shader(ShaderType type, const std::vector<char> &rsd_code) noexcept : m_type(type) {

    nlohmann::json json_data = nlohmann::json::parse(rsd_code);
    DescriptorDesc desc{};

    for (auto &frequency : json_data.items()) {
        if (frequency.key() == "UPDATE_FREQ_NONE") {
            desc.update_frequency = ResourceUpdateFrequency::NONE;
        } else if (frequency.key() == "UPDATE_FREQ_PER_FRAME") {
            desc.update_frequency = ResourceUpdateFrequency::PER_FRAME;
        } else if (frequency.key() == "UPDATE_FREQ_PER_BATCH") {
            desc.update_frequency = ResourceUpdateFrequency::PER_BATCH;
        } else if (frequency.key() == "UPDATE_FREQ_PER_DRAW") {
            desc.update_frequency = ResourceUpdateFrequency::PER_DRAW;
        } else {
            LOG_ERROR("invalid frequency");
        }

        for (auto &descriptor : frequency.value()) {
            desc.name = descriptor["name"].get<std::string>();
            desc.type = static_cast<DescriptorType>(descriptor["type"].get<u32>());
            desc.vk_binding = descriptor["vk_binding"];
            // auto reg = descriptor["dx_register"]; TODO
            rsd.descs.push_back(desc);

            vk_binding_count[static_cast<u32>(desc.update_frequency)] =
                max(vk_binding_count[static_cast<u32>(desc.update_frequency)], desc.vk_binding + 1);
        }
    }
}

ShaderType Shader::GetType() const noexcept { return m_type; }

const std::string &Shader::GetEntryPoint() const noexcept { return m_entry_point; }
} // namespace Horizon::RHI