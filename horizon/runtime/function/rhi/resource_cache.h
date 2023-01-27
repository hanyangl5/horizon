#pragma once
#include <runtime/core/utils/definations.h>
#include <runtime/core/utils/functions.h>
#include <runtime/function/rhi/rhi_utils.h>
#include <vulkan/vulkan.h>

namespace std {

template <> struct hash<VkDescriptorSetLayoutCreateInfo> {
    inline Horizon::u64 operator()(VkDescriptorSetLayoutCreateInfo const &layout) const {
        Horizon::u64 seed = 0;
        Horizon::HashCombine(seed, layout.bindingCount);
        for (Horizon::u32 i = 0; i < layout.bindingCount; i++) {
            Horizon::HashCombine(seed, layout.pBindings[i].binding);
            Horizon::HashCombine(seed, layout.pBindings[i].descriptorCount);
            Horizon::HashCombine(seed, layout.pBindings[i].stageFlags);
            Horizon::HashCombine(seed, layout.pBindings[i].descriptorType);
        }
        return seed;
    }
};
template <> struct hash<Horizon::GraphicsPipelineCreateInfo> {
    inline Horizon::u64 operator()(const Horizon::GraphicsPipelineCreateInfo &create_info) const {
        Horizon::u64 seed = 0;
        Horizon::HashCombine(seed, create_info.depth_stencil_state.depthNear);
        Horizon::HashCombine(seed, create_info.depth_stencil_state.depthFar);
        Horizon::HashCombine(seed, create_info.depth_stencil_state.depth_func);
        Horizon::HashCombine(seed, create_info.depth_stencil_state.depth_stencil_format);
        Horizon::HashCombine(seed, create_info.depth_stencil_state.depth_test);
        Horizon::HashCombine(seed, create_info.depth_stencil_state.depth_write);
        //
        return seed;
    }
};
//template <> struct hash<Horizon::ComputePipelineCreateInfo> {
//    inline Horizon::u64 operator()(const Horizon::ComputePipelineCreateInfo &create_info) const {
//        Horizon::u64 seed = 0;
//        //
//        return seed;
//    }
//};

//template <> struct hash<Horizon::PipelineCreateInfo> {
//    inline Horizon::u64 operator()(const Horizon::PipelineCreateInfo &create_info) const {
//        Horizon::u64 seed = 0;
//        switch (create_info.type) {
//        case Horizon::PipelineType::GRAPHICS:
//            Horizon::HashCombine(seed, *create_info.gpci);
//            break;
//        case Horizon::PipelineType::COMPUTE:
//            Horizon::HashCombine(seed, *create_info.cpci);
//            break;
//        default:
//            break;
//        };
//        Horizon::HashCombine(seed, create_info.type);
//        return seed;
//    }
//};


} // namespace std

//
//namespace std {
//template <> struct hash<ShaderSource> {
//    Horizon::u64 operator()(const ShaderSource &shader_source) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, shader_source.get_id());
//
//        return result;
//    }
//};
//
//template <> struct hash<ShaderVariant> {
//    Horizon::u64 operator()(const ShaderVariant &shader_variant) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, shader_variant.get_id());
//
//        return result;
//    }
//};
//
//template <> struct hash<ShaderModule> {
//    Horizon::u64 operator()(const ShaderModule &shader_module) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, shader_module.get_id());
//
//        return result;
//    }
//};
//
//template <> struct hash<DescriptorSetLayout> {
//    Horizon::u64 operator()(const DescriptorSetLayout &descriptor_set_layout) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, descriptor_set_layout.get_handle());
//
//        return result;
//    }
//};
//
//template <> struct hash<DescriptorPool> {
//    Horizon::u64 operator()(const DescriptorPool &descriptor_pool) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, descriptor_pool.get_descriptor_set_layout());
//
//        return result;
//    }
//};
//
//template <> struct hash<PipelineLayout> {
//    Horizon::u64 operator()(const PipelineLayout &pipeline_layout) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, pipeline_layout.get_handle());
//
//        return result;
//    }
//};
//
//template <> struct hash<RenderPass> {
//    Horizon::u64 operator()(const RenderPass &render_pass) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, render_pass.get_handle());
//
//        return result;
//    }
//};
//
//template <> struct hash<Attachment> {
//    Horizon::u64 operator()(const Attachment &attachment) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkFormat>::type>(attachment.format));
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkSampleCountFlagBits>::type>(attachment.samples));
//        Horizon::HashCombine(result, attachment.usage);
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkImageLayout>::type>(attachment.initial_layout));
//
//        return result;
//    }
//};
//
//template <> struct hash<Horizon::RenderTargetLoadOp> {
//    Horizon::u64 operator()(const Horizon::RenderTargetLoadOp &load_info) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, static_cast<Horizon::i32>(load_info));
//
//        return result;
//    }
//};
//
//template <> struct hash<Horizon::RenderTargetStoreOp> {
//    Horizon::u64 operator()(const Horizon::RenderTargetStoreOp &store_info) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, static_cast<Horizon::i32>(store_info));
//
//        return result;
//    }
//};
//
//template <> struct hash<ShaderResource> {
//    Horizon::u64 operator()(const ShaderResource &shader_resource) const {
//        Horizon::u64 result = 0;
//
//        if (shader_resource.type == ShaderResourceType::Input ||
//            shader_resource.type == ShaderResourceType::Output ||
//            shader_resource.type == ShaderResourceType::PushConstant ||
//            shader_resource.type == ShaderResourceType::SpecializationConstant) {
//            return result;
//        }
//
//        Horizon::HashCombine(result, shader_resource.set);
//        Horizon::HashCombine(result, shader_resource.binding);
//        Horizon::HashCombine(result,
//                          static_cast<std::underlying_type<ShaderResourceType>::type>(shader_resource.type));
//        Horizon::HashCombine(result, shader_resource.mode);
//
//        return result;
//    }
//};
//
//template <> struct hash<VkDescriptorBufferInfo> {
//    Horizon::u64 operator()(const VkDescriptorBufferInfo &descriptor_buffer_info) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, descriptor_buffer_info.buffer);
//        Horizon::HashCombine(result, descriptor_buffer_info.range);
//        Horizon::HashCombine(result, descriptor_buffer_info.offset);
//
//        return result;
//    }
//};
//
//template <> struct hash<VkDescriptorImageInfo> {
//    Horizon::u64 operator()(const VkDescriptorImageInfo &descriptor_image_info) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, descriptor_image_info.imageView);
//        Horizon::HashCombine(result,
//                          static_cast<std::underlying_type<VkImageLayout>::type>(descriptor_image_info.imageLayout));
//        Horizon::HashCombine(result, descriptor_image_info.sampler);
//
//        return result;
//    }
//};
//
//template <> struct hash<VkWriteDescriptorSet> {
//    Horizon::u64 operator()(const VkWriteDescriptorSet &write_descriptor_set) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, write_descriptor_set.dstSet);
//        Horizon::HashCombine(result, write_descriptor_set.dstBinding);
//        Horizon::HashCombine(result, write_descriptor_set.dstArrayElement);
//        Horizon::HashCombine(result, write_descriptor_set.descriptorCount);
//        Horizon::HashCombine(result, write_descriptor_set.descriptorType);
//
//        switch (write_descriptor_set.descriptorType) {
//        case VK_DESCRIPTOR_TYPE_SAMPLER:
//        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
//        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
//        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
//        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
//            for (Horizon::u32 i = 0; i < write_descriptor_set.descriptorCount; i++) {
//                Horizon::HashCombine(result, write_descriptor_set.pImageInfo[i]);
//            }
//            break;
//
//        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
//        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
//            for (Horizon::u32 i = 0; i < write_descriptor_set.descriptorCount; i++) {
//                Horizon::HashCombine(result, write_descriptor_set.pTexelBufferView[i]);
//            }
//            break;
//
//        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
//        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
//        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
//        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
//            for (Horizon::u32 i = 0; i < write_descriptor_set.descriptorCount; i++) {
//                Horizon::HashCombine(result, write_descriptor_set.pBufferInfo[i]);
//            }
//            break;
//
//        default:
//            // Not implemented
//            break;
//        };
//
//        return result;
//    }
//};
//
//template <> struct hash<VkVertexInputAttributeDescription> {
//    Horizon::u64 operator()(const VkVertexInputAttributeDescription &vertex_attrib) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, vertex_attrib.binding);
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkFormat>::type>(vertex_attrib.format));
//        Horizon::HashCombine(result, vertex_attrib.location);
//        Horizon::HashCombine(result, vertex_attrib.offset);
//
//        return result;
//    }
//};
//
//template <> struct hash<VkVertexInputBindingDescription> {
//    Horizon::u64 operator()(const VkVertexInputBindingDescription &vertex_binding) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, vertex_binding.binding);
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkVertexInputRate>::type>(vertex_binding.inputRate));
//        Horizon::HashCombine(result, vertex_binding.stride);
//
//        return result;
//    }
//};
//
//template <> struct hash<StencilOpState> {
//    Horizon::u64 operator()(const StencilOpState &stencil) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkCompareOp>::type>(stencil.compare_op));
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkStencilOp>::type>(stencil.depth_fail_op));
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkStencilOp>::type>(stencil.fail_op));
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkStencilOp>::type>(stencil.pass_op));
//
//        return result;
//    }
//};
//
//template <> struct hash<VkExtent2D> {
//    size_t operator()(const VkExtent2D &extent) const {
//        size_t result = 0;
//
//        Horizon::HashCombine(result, extent.width);
//        Horizon::HashCombine(result, extent.height);
//
//        return result;
//    }
//};
//
//template <> struct hash<VkOffset2D> {
//    size_t operator()(const VkOffset2D &offset) const {
//        size_t result = 0;
//
//        Horizon::HashCombine(result, offset.x);
//        Horizon::HashCombine(result, offset.y);
//
//        return result;
//    }
//};
//
//template <> struct hash<VkRect2D> {
//    size_t operator()(const VkRect2D &rect) const {
//        size_t result = 0;
//
//        Horizon::HashCombine(result, rect.extent);
//        Horizon::HashCombine(result, rect.offset);
//
//        return result;
//    }
//};
//
//template <> struct hash<VkViewport> {
//    size_t operator()(const VkViewport &viewport) const {
//        size_t result = 0;
//
//        Horizon::HashCombine(result, viewport.width);
//        Horizon::HashCombine(result, viewport.height);
//        Horizon::HashCombine(result, viewport.maxDepth);
//        Horizon::HashCombine(result, viewport.minDepth);
//        Horizon::HashCombine(result, viewport.x);
//        Horizon::HashCombine(result, viewport.y);
//
//        return result;
//    }
//};
//
//template <> struct hash<ColorBlendAttachmentState> {
//    Horizon::u64 operator()(const ColorBlendAttachmentState &color_blend_attachment) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result,
//                          static_cast<std::underlying_type<VkBlendOp>::type>(color_blend_attachment.alpha_blend_op));
//        Horizon::HashCombine(result, color_blend_attachment.blend_enable);
//        Horizon::HashCombine(result,
//                          static_cast<std::underlying_type<VkBlendOp>::type>(color_blend_attachment.color_blend_op));
//        Horizon::HashCombine(result, color_blend_attachment.color_write_mask);
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkBlendFactor>::type>(
//                                      color_blend_attachment.dst_alpha_blend_factor));
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkBlendFactor>::type>(
//                                      color_blend_attachment.dst_color_blend_factor));
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkBlendFactor>::type>(
//                                      color_blend_attachment.src_alpha_blend_factor));
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkBlendFactor>::type>(
//                                      color_blend_attachment.src_color_blend_factor));
//
//        return result;
//    }
//};
//
//template <> struct hash<RenderTarget> {
//    Horizon::u64 operator()(const RenderTarget &render_target) const {
//        Horizon::u64 result = 0;
//
//        for (auto &view : render_target.get_views()) {
//            Horizon::HashCombine(result, view.get_handle());
//            Horizon::HashCombine(result, view.get_image().get_handle());
//        }
//
//        return result;
//    }
//};
//
//template <> struct hash<PipelineState> {
//    Horizon::u64 operator()(const PipelineState &pipeline_state) const {
//        Horizon::u64 result = 0;
//
//        Horizon::HashCombine(result, pipeline_state.get_pipeline_layout().get_handle());
//
//        // For graphics only
//        if (auto render_pass = pipeline_state.get_render_pass()) {
//            Horizon::HashCombine(result, render_pass->get_handle());
//        }
//
//        Horizon::HashCombine(result, pipeline_state.get_specialization_constant_state());
//
//        Horizon::HashCombine(result, pipeline_state.get_subpass_index());
//
//        for (auto shader_module : pipeline_state.get_pipeline_layout().get_shader_modules()) {
//            Horizon::HashCombine(result, shader_module->get_id());
//        }
//
//        // VkPipelineVertexInputStateCreateInfo
//        for (auto &attribute : pipeline_state.get_vertex_input_state().attributes) {
//            Horizon::HashCombine(result, attribute);
//        }
//
//        for (auto &binding : pipeline_state.get_vertex_input_state().bindings) {
//            Horizon::HashCombine(result, binding);
//        }
//
//        // VkPipelineInputAssemblyStateCreateInfo
//        Horizon::HashCombine(result, pipeline_state.get_input_assembly_state().primitive_restart_enable);
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkPrimitiveTopology>::type>(
//                                      pipeline_state.get_input_assembly_state().topology));
//
//        //VkPipelineViewportStateCreateInfo
//        Horizon::HashCombine(result, pipeline_state.get_viewport_state().viewport_count);
//        Horizon::HashCombine(result, pipeline_state.get_viewport_state().scissor_count);
//
//        // VkPipelineRasterizationStateCreateInfo
//        Horizon::HashCombine(result, pipeline_state.get_rasterization_state().cull_mode);
//        Horizon::HashCombine(result, pipeline_state.get_rasterization_state().depth_bias_enable);
//        Horizon::HashCombine(result, pipeline_state.get_rasterization_state().depth_clamp_enable);
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkFrontFace>::type>(
//                                      pipeline_state.get_rasterization_state().front_face));
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkPolygonMode>::type>(
//                                      pipeline_state.get_rasterization_state().polygon_mode));
//        Horizon::HashCombine(result, pipeline_state.get_rasterization_state().rasterizer_discard_enable);
//
//        // VkPipelineMultisampleStateCreateInfo
//        Horizon::HashCombine(result, pipeline_state.get_multisample_state().alpha_to_coverage_enable);
//        Horizon::HashCombine(result, pipeline_state.get_multisample_state().alpha_to_one_enable);
//        Horizon::HashCombine(result, pipeline_state.get_multisample_state().min_sample_shading);
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkSampleCountFlagBits>::type>(
//                                      pipeline_state.get_multisample_state().rasterization_samples));
//        Horizon::HashCombine(result, pipeline_state.get_multisample_state().sample_shading_enable);
//        Horizon::HashCombine(result, pipeline_state.get_multisample_state().sample_mask);
//
//        // VkPipelineDepthStencilStateCreateInfo
//        Horizon::HashCombine(result, pipeline_state.get_depth_stencil_state().back);
//        Horizon::HashCombine(result, pipeline_state.get_depth_stencil_state().depth_bounds_test_enable);
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkCompareOp>::type>(
//                                      pipeline_state.get_depth_stencil_state().depth_compare_op));
//        Horizon::HashCombine(result, pipeline_state.get_depth_stencil_state().depth_test_enable);
//        Horizon::HashCombine(result, pipeline_state.get_depth_stencil_state().depth_write_enable);
//        Horizon::HashCombine(result, pipeline_state.get_depth_stencil_state().front);
//        Horizon::HashCombine(result, pipeline_state.get_depth_stencil_state().stencil_test_enable);
//
//        // VkPipelineColorBlendStateCreateInfo
//        Horizon::HashCombine(result, static_cast<std::underlying_type<VkLogicOp>::type>(
//                                      pipeline_state.get_color_blend_state().logic_op));
//        Horizon::HashCombine(result, pipeline_state.get_color_blend_state().logic_op_enable);
//
//        for (auto &attachment : pipeline_state.get_color_blend_state().attachments) {
//            Horizon::HashCombine(result, attachment);
//        }
//
//        return result;
//    }
//};
//} // namespace std
//
//namespace vkb {
//namespace {
//template <typename T> inline void hash_param(size_t &seed, const T &value) { Horizon::HashCombine(seed, value); }
//
//template <> inline void hash_param(size_t & /*seed*/, const VkPipelineCache & /*value*/) {}
//
//template <> inline void hash_param<std::vector<uint8_t>>(size_t &seed, const std::vector<uint8_t> &value) {
//    Horizon::HashCombine(seed, std::string{value.begin(), value.end()});
//}
//
//template <> inline void hash_param<std::vector<Attachment>>(size_t &seed, const std::vector<Attachment> &value) {
//    for (auto &attachment : value) {
//        Horizon::HashCombine(seed, attachment);
//    }
//}
//
//template <> inline void hash_param<std::vector<LoadStoreInfo>>(size_t &seed, const std::vector<LoadStoreInfo> &value) {
//    for (auto &load_store_info : value) {
//        Horizon::HashCombine(seed, load_store_info);
//    }
//}
//
//template <> inline void hash_param<std::vector<SubpassInfo>>(size_t &seed, const std::vector<SubpassInfo> &value) {
//    for (auto &subpass_info : value) {
//        Horizon::HashCombine(seed, subpass_info);
//    }
//}
//
//template <>
//inline void hash_param<std::vector<ShaderModule *>>(size_t &seed, const std::vector<ShaderModule *> &value) {
//    for (auto &shader_module : value) {
//        Horizon::HashCombine(seed, shader_module->get_id());
//    }
//}
//
//template <>
//inline void hash_param<std::vector<ShaderResource>>(size_t &seed, const std::vector<ShaderResource> &value) {
//    for (auto &resource : value) {
//        Horizon::HashCombine(seed, resource);
//    }
//}
//
//template <>
//inline void hash_param<Horizon::Container::Map<Horizon::u32, Container::Map<Horizon::u32, VkDescriptorBufferInfo>>>(
//    size_t &seed, const Container::Map<Horizon::u32, Container::Map<Horizon::u32, VkDescriptorBufferInfo>> &value) {
//    for (auto &binding_set : value) {
//        Horizon::HashCombine(seed, binding_set.first);
//
//        for (auto &binding_element : binding_set.second) {
//            Horizon::HashCombine(seed, binding_element.first);
//            Horizon::HashCombine(seed, binding_element.second);
//        }
//    }
//}
//
//template <>
//inline void hash_param<Container::Map<Horizon::u32, Container::Map<Horizon::u32, VkDescriptorImageInfo>>>(
//    size_t &seed, const Container::Map<Horizon::u32, Container::Map<Horizon::u32, VkDescriptorImageInfo>> &value) {
//    for (auto &binding_set : value) {
//        Horizon::HashCombine(seed, binding_set.first);
//
//        for (auto &binding_element : binding_set.second) {
//            Horizon::HashCombine(seed, binding_element.first);
//            Horizon::HashCombine(seed, binding_element.second);
//        }
//    }
//}
//
//template <typename T, typename... Args> inline void hash_param(size_t &seed, const T &first_arg, const Args &...args) {
//    hash_param(seed, first_arg);
//
//    hash_param(seed, args...);
//}
//
//template <class T, class... A> struct RecordHelper {
//    size_t record(ResourceRecord & /*recorder*/, A &.../*args*/) { return 0; }
//
//    void index(ResourceRecord & /*recorder*/, size_t /*index*/, T & /*resource*/) {}
//};
//
//template <class... A> struct RecordHelper<ShaderModule, A...> {
//    size_t record(ResourceRecord &recorder, A &...args) { return recorder.register_shader_module(args...); }
//
//    void index(ResourceRecord &recorder, size_t index, ShaderModule &shader_module) {
//        recorder.set_shader_module(index, shader_module);
//    }
//};
//
//template <class... A> struct RecordHelper<PipelineLayout, A...> {
//    size_t record(ResourceRecord &recorder, A &...args) { return recorder.register_pipeline_layout(args...); }
//
//    void index(ResourceRecord &recorder, size_t index, PipelineLayout &pipeline_layout) {
//        recorder.set_pipeline_layout(index, pipeline_layout);
//    }
//};
//
//template <class... A> struct RecordHelper<RenderPass, A...> {
//    size_t record(ResourceRecord &recorder, A &...args) { return recorder.register_render_pass(args...); }
//
//    void index(ResourceRecord &recorder, size_t index, RenderPass &render_pass) {
//        recorder.set_render_pass(index, render_pass);
//    }
//};
//
//template <class... A> struct RecordHelper<GraphicsPipeline, A...> {
//    size_t record(ResourceRecord &recorder, A &...args) { return recorder.register_graphics_pipeline(args...); }
//
//    void index(ResourceRecord &recorder, size_t index, GraphicsPipeline &graphics_pipeline) {
//        recorder.set_graphics_pipeline(index, graphics_pipeline);
//    }
//};
//} // namespace
//
//template <class T, class... A>
//T &request_resource(Device &device, ResourceRecord *recorder, std::unordered_map<Horizon::u64, T> &resources,
//                    A &...args) {
//    RecordHelper<T, A...> record_helper;
//
//    Horizon::u64 hash{0U};
//    hash_param(hash, args...);
//
//    auto res_it = resources.find(hash);
//
//    if (res_it != resources.end()) {
//        return res_it->second;
//    }
//
//    // If we do not have it already, create and cache it
//    const char *res_type = typeid(T).name();
//    size_t res_id = resources.size();
//
//    LOGD("Building #{} cache object ({})", res_id, res_type);
//
//// Only error handle in release
//#ifndef DEBUG
//    try {
//#endif
//        T resource(device, args...);
//
//        auto res_ins_it = resources.emplace(hash, std::move(resource));
//
//        if (!res_ins_it.second) {
//            throw std::runtime_error{std::string{"Insertion error for #"} + std::to_string(res_id) + "cache object (" +
//                                     res_type + ")"};
//        }
//
//        res_it = res_ins_it.first;
//
//        if (recorder) {
//            size_t index = record_helper.record(*recorder, args...);
//            record_helper.index(*recorder, index, res_it->second);
//        }
//#ifndef DEBUG
//    } catch (const std::exception &e) {
//        LOGE("Creation error for #{} cache object ({})", res_id, res_type);
//        throw e;
//    }
//#endif
//
//    return res_it->second;
//}