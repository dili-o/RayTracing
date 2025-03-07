#include "GPUResources.hpp"

#include "GPUDevice.hpp"

#include <Core/Assert.hpp>

namespace Helix {


    // DepthStencilCreation ////////////////////////////////////

    DepthStencilCreation& DepthStencilCreation::set_depth(bool write, VkCompareOp comparison_test) {
        depth_write_enable = write;
        depth_comparison = comparison_test;
        // Setting depth like this means we want to use the depth test.
        depth_enable = 1;

        return *this;
    }

    // BlendState  /////////////////////////////////////////////
    BlendState& BlendState::set_color(VkBlendFactor source, VkBlendFactor destination, VkBlendOp operation) {
        source_color = source;
        destination_color = destination;
        color_operation = operation;
        blend_enabled = 1;

        return *this;
    }

    BlendState& BlendState::set_alpha(VkBlendFactor source, VkBlendFactor destination, VkBlendOp operation) {
        source_alpha = source;
        destination_alpha = destination;
        alpha_operation = operation;
        separate_blend = 1;

        return *this;
    }

    BlendState& BlendState::set_color_write_mask(ColorWriteEnabled::Mask value) {
        color_write_mask = value;

        return *this;
    }

    // BlendStateCreation //////////////////////////////////////
    BlendStateCreation& BlendStateCreation::reset() {
        active_states = 0;

        return *this;
    }

    BlendState& BlendStateCreation::add_blend_state() {
        return blend_states[active_states++];
    }

    // BufferCreation //////////////////////////////////////////
    BufferCreation& BufferCreation::reset() {

        type_flags = 0;
        usage = ResourceUsageType::Immutable;
        size = 0;
        persistent = 0;
        device_only = 0;
        initial_data = nullptr;
        
        name = nullptr;

        return *this;
    }

    BufferCreation& BufferCreation::set(VkBufferUsageFlags flags, ResourceUsageType::Enum usage_, u32 size_) {
        type_flags = flags;
        usage = usage_;
        size = size_;

        return *this;
    }

    BufferCreation& BufferCreation::set_data(void* data_) {
        initial_data = data_;

        return *this;
    }

    BufferCreation& BufferCreation::set_name(cstring name_) {
        name = name_;

        return *this;
    }

    BufferCreation& BufferCreation::set_persistent(bool value) {
        persistent = value ? 1 : 0;
        return *this;
    }

    BufferCreation& BufferCreation::set_device_only(bool value) {
        device_only = value ? 1 : 0;
        return *this;
    }

    // TextureCreation /////////////////////////////////////////
    TextureCreation& TextureCreation::set_size(u16 width_, u16 height_, u16 depth_) {
        width = width_;
        height = height_;
        depth = depth_;

        return *this;
    }

    TextureCreation& TextureCreation::set_flags(u8 mipmaps_, u8 flags_) {
        mip_level_count = mipmaps_;
        flags = flags_;

        return *this;
    }

    TextureCreation& TextureCreation::set_layers(u32 layer_count_) {
        array_layer_count = layer_count_;

        return *this;
    }

    TextureCreation& TextureCreation::set_format_type(VkFormat format_, TextureType::Enum type_) {
        format = format_;
        type = type_;

        return *this;
    }

    TextureCreation& TextureCreation::set_name(cstring name_) {
        name = name_;

        return *this;
    }

    TextureCreation& TextureCreation::set_data(void* data_) {
        initial_data = data_;

        return *this;
    }

    TextureCreation& TextureCreation::set_alias(TextureHandle alias_) {
        alias = alias_;

        return *this;
    }

    // TextureViewCreation ////////////////////////////////////////////////////
    TextureViewCreation& TextureViewCreation::set_parent_texture(TextureHandle parent_texture_) {
        parent_texture = parent_texture_;

        return *this;
    }

    TextureViewCreation& TextureViewCreation::set_mips(u32 base_mip_, u32 mip_level_count_) {
        mip_base_level = base_mip_;
        mip_level_count = mip_level_count_;

        return *this;
    }

    TextureViewCreation& TextureViewCreation::set_array(u32 base_layer_, u32 layer_count_) {
        array_base_layer = base_layer_;
        array_layer_count = layer_count_;

        return *this;
    }

    TextureViewCreation& TextureViewCreation::set_name(cstring name_) {
        name = name_;

        return *this;
    }

    // SamplerCreation /////////////////////////////////////////
    SamplerCreation& SamplerCreation::set_min_mag_mip(VkFilter min, VkFilter mag, VkSamplerMipmapMode mip) {
        min_filter = min;
        mag_filter = mag;
        mip_filter = mip;

        return *this;
    }

    SamplerCreation& SamplerCreation::set_address_mode_u(VkSamplerAddressMode u) {
        address_mode_u = u;

        return *this;
    }

    SamplerCreation& SamplerCreation::set_address_mode_uv(VkSamplerAddressMode u, VkSamplerAddressMode v) {
        address_mode_u = u;
        address_mode_v = v;

        return *this;
    }

    SamplerCreation& SamplerCreation::set_address_mode_uvw(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w) {
        address_mode_u = u;
        address_mode_v = v;
        address_mode_w = w;

        return *this;
    }

    SamplerCreation& SamplerCreation::set_reduction_mode(VkSamplerReductionMode mode) {
        reduction_mode = mode;

        return *this;
    }

    SamplerCreation& SamplerCreation::set_name(cstring name_) {
        name = name_;

        return *this;
    }


    // ShaderStateCreation /////////////////////////////////////
    ShaderStateCreation& ShaderStateCreation::reset() {
        stages_count = 0;

        return *this;
    }

    ShaderStateCreation& ShaderStateCreation::set_name(cstring name_) {
        name = name_;

        return *this;
    }

    ShaderStateCreation& ShaderStateCreation::add_stage(cstring code, u32 code_size, VkShaderStageFlagBits type) {
        stages[stages_count].code = code;
        stages[stages_count].code_size = code_size;
        stages[stages_count].type = type;
        ++stages_count;

        return *this;
    }

    ShaderStateCreation& ShaderStateCreation::set_spv_input(bool value) {
        spv_input = value;
        return *this;
    }

    // DescriptorSetLayoutCreation ////////////////////////////////////////////
    DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::reset() {
        num_bindings = 0;
        set_index = 0;
        return *this;
    }

    DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::add_binding(const Binding& binding) {
        bindings[num_bindings++] = binding;
        return *this;
    }

    DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::add_binding_at_index(const Binding& binding, int index) {
        bindings[index] = binding;
        num_bindings = ((u32)index + 1) > num_bindings ? (index + 1) : num_bindings;
        return *this;
    }

    DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::set_name(cstring name_) {
        name = name_;
        return *this;
    }


    DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::set_set_index(u32 index) {
        set_index = index;
        return *this;
    }

    void DescriptorSetLayoutCreation::sort_bindings_by_index() {
        auto compare_function = [](void const* a, void const* b) -> int {
            const Binding* binding_a = (Binding*)a;
            const Binding* binding_b = (Binding*)b;

            if (binding_a->index < binding_b->index)
                return -1;
            if (binding_a->index > binding_b->index)
                return 1;
            return 0;
        };

        qsort(
            bindings,
            num_bindings,
            sizeof(DescriptorSetLayoutCreation::Binding),
            compare_function);
    }

    // DescriptorSetCreation //////////////////////////////////////////////////
    DescriptorSetCreation& DescriptorSetCreation::reset() {
        num_resources = 0;
        return *this;
    }

    DescriptorSetCreation& DescriptorSetCreation::set_layout(DescriptorSetLayoutHandle layout_) {
        layout = layout_;
        return *this;
    }

    DescriptorSetCreation& DescriptorSetCreation::texture(TextureHandle texture, u16 binding) {
        // Set a default sampler
        samplers[num_resources] = k_invalid_sampler;
        bindings[num_resources] = binding;
        resources[num_resources++] = texture.index;
        return *this;
    }

    DescriptorSetCreation& DescriptorSetCreation::buffer(BufferHandle buffer, u16 binding) {
        samplers[num_resources] = k_invalid_sampler;
        bindings[num_resources] = binding;
        resources[num_resources++] = buffer.index;
        return *this;
    }

    DescriptorSetCreation& DescriptorSetCreation::texture_sampler(TextureHandle texture, SamplerHandle sampler, u16 binding) {
        bindings[num_resources] = binding;
        resources[num_resources] = texture.index;
        samplers[num_resources++] = sampler;
        return *this;
    }

    DescriptorSetCreation& DescriptorSetCreation::set_name(cstring name_) {
        name = name_;
        return *this;
    }

    void DescriptorSetCreation::sort_bindings_by_index() {
        std::vector<u16> indices(num_resources);

        for (int i = 0; i < num_resources; ++i) {
            indices[i] = i;
        }

        std::sort(indices.begin(), indices.end(), [&](u16 i, u16 j) {
            return bindings[i] < bindings[j];
        });

        ResourceHandle sorted_resources[k_max_descriptors_per_set];
        SamplerHandle  sorted_samplers[k_max_descriptors_per_set];
        u16 sorted_bindings[k_max_descriptors_per_set];

        for (u32 i = 0; i < num_resources; ++i) {
            sorted_resources[i] = resources[indices[i]];
            sorted_samplers[i] = samplers[indices[i]];
            sorted_bindings[i] = bindings[indices[i]];
        }

        memcpy(resources, sorted_resources, num_resources * sizeof(ResourceHandle));
        memcpy(samplers, sorted_samplers, num_resources * sizeof(SamplerHandle));
        memcpy(bindings, sorted_bindings, num_resources * sizeof(u16));
    }

    // VertexInputCreation /////////////////////////////////////
    VertexInputCreation& VertexInputCreation::reset() {
        num_vertex_streams = num_vertex_attributes = 0;
        return *this;
    }

    VertexInputCreation& VertexInputCreation::add_vertex_stream(const VertexStream& stream) {
        vertex_streams[num_vertex_streams++] = stream;
        return *this;
    }

    VertexInputCreation& VertexInputCreation::add_vertex_attribute(const VertexAttribute& attribute) {
        vertex_attributes[num_vertex_attributes++] = attribute;
        return *this;
    }

    // RenderPassOutput ////////////////////////////////////////
    RenderPassOutput& RenderPassOutput::reset() {
        num_color_formats = 0;
        for (u32 i = 0; i < k_max_image_outputs; ++i) {
            color_formats[i] = VK_FORMAT_UNDEFINED;
            color_final_layouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
            color_operations[i] = RenderPassOperation::DontCare;
        }
        depth_stencil_format = VK_FORMAT_UNDEFINED;
        depth_operation = stencil_operation = RenderPassOperation::DontCare;
        return *this;
    }

    RenderPassOutput& RenderPassOutput::color(VkFormat format, VkImageLayout final_layout, RenderPassOperation::Enum load_op) {
        color_formats[num_color_formats] = format;
        color_operations[num_color_formats] = load_op;
        color_final_layouts[num_color_formats++] = final_layout;
        return *this;
    }

    RenderPassOutput& RenderPassOutput::depth_stencil(VkFormat format, VkImageLayout final_layout) {
        depth_stencil_format = format;
        depth_stencil_final_layout = final_layout;
        return *this;
    }

    RenderPassOutput& RenderPassOutput::set_depth_stencil_operations(RenderPassOperation::Enum depth_, RenderPassOperation::Enum stencil_) {
        depth_operation = depth_;
        stencil_operation = stencil_;

        return *this;
    }

    // PipelineCreation ////////////////////////////////////////
    PipelineCreation& PipelineCreation::reset() {
        rasterization_creation = RasterizationCreation{};
        depth_stencil_creation = DepthStencilCreation{};
        blend_state_creation.reset();
        vertex_input_creation.reset();
        shader_state_creation.reset();
        render_pass.reset();
        viewport = nullptr;
        name = nullptr;

        for (u32 i = 0; i < num_active_layouts; i++) {
            descriptor_set_layout[i].index = k_invalid_index;
        }

        num_active_layouts = 0;
        return *this;
    }

    PipelineCreation& PipelineCreation::add_descriptor_set_layout(DescriptorSetLayoutHandle handle) {
        descriptor_set_layout[num_active_layouts++] = handle;
        return *this;
    }

    RenderPassOutput& PipelineCreation::render_pass_output() {
        return render_pass;
    }

    // RenderPassCreation //////////////////////////////////////
    RenderPassCreation& RenderPassCreation::reset() {
        num_render_targets = 0;
        depth_stencil_format = VK_FORMAT_UNDEFINED;
        for (u32 i = 0; i < k_max_image_outputs; ++i) {
            color_operations[i] = RenderPassOperation::DontCare;
        }
        depth_operation = stencil_operation = RenderPassOperation::DontCare;

        return *this;
    }

    RenderPassCreation& RenderPassCreation::add_attachment(VkFormat format, VkImageLayout layout, RenderPassOperation::Enum load_op) {
        color_formats[num_render_targets] = format;
        color_operations[num_render_targets] = load_op;
        color_final_layouts[num_render_targets++] = layout;

        return *this;
    }

    RenderPassCreation& RenderPassCreation::set_depth_stencil(VkFormat format, VkImageLayout layout) {
        depth_stencil_format = format;
        depth_stencil_final_layout = layout;

        return *this;
    }

    RenderPassCreation& RenderPassCreation::set_name(cstring name_) {
        name = name_;

        return *this;
    }

    RenderPassCreation& RenderPassCreation::set_depth_stencil_operations(RenderPassOperation::Enum depth_, RenderPassOperation::Enum stencil_) {
        depth_operation = depth_;
        stencil_operation = stencil_;

        return *this;
    }

    // FramebufferCreation //////////////////////////////////////
    FramebufferCreation& FramebufferCreation::reset()
    {
        num_render_targets = 0;
        name = nullptr;
        depth_stencil_texture.index = k_invalid_index;

        resize = 0;
        scale_x = 1.f;
        scale_y = 1.f;

        return *this;
    }

    FramebufferCreation& FramebufferCreation::add_render_texture(TextureHandle texture)
    {
        output_textures[num_render_targets++] = texture;

        return *this;
    }

    FramebufferCreation& FramebufferCreation::set_depth_stencil_texture(TextureHandle texture)
    {
        depth_stencil_texture = texture;

        return *this;
    }

    FramebufferCreation& FramebufferCreation::set_scaling(f32 scale_x_, f32 scale_y_, u8 resize_) {
        scale_x = scale_x_;
        scale_y = scale_y_;
        resize = resize_;

        return *this;
    }

    FramebufferCreation& FramebufferCreation::set_name(const char* name_)
    {
        name = name_;

        return *this;
    }

    // ExecutionBarrier ////////////////////////////////////////
    ExecutionBarrier& ExecutionBarrier::reset() {
        num_image_barriers = num_buffer_barriers = 0;
        source_pipeline_stage = PipelineStage::DrawIndirect;
        destination_pipeline_stage = PipelineStage::DrawIndirect;
        return *this;
    }

    ExecutionBarrier& ExecutionBarrier::set(PipelineStage::Enum source, PipelineStage::Enum destination) {
        source_pipeline_stage = source;
        destination_pipeline_stage = destination;

        return *this;
    }

    ExecutionBarrier& ExecutionBarrier::add_image_barrier(const ImageBarrier& image_barrier) {
        image_barriers[num_image_barriers++] = image_barrier;

        return *this;
    }

    ExecutionBarrier& ExecutionBarrier::add_buffer_barrier(const BufferBarrier& buffer_barrier) {
        buffer_barriers[num_buffer_barriers++] = buffer_barrier;

        return *this;
    }

    //
    // Utils


    void util_add_image_barrier(GpuDevice* gpu, VkCommandBuffer command_buffer, Texture* texture, ResourceState new_state,
        u32 base_mip_level, u32 mip_count, bool is_depth, u32 source_family, u32 destination_family,
        QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type){
        util_add_image_barrier(gpu, command_buffer, texture->vk_image, texture->state, new_state, base_mip_level, mip_count, is_depth, source_family,
            destination_family, source_queue_type, destination_queue_type);
        texture->state = new_state;
    }

    void util_add_image_barrier(GpuDevice* gpu, VkCommandBuffer command_buffer, VkImage image, ResourceState old_state, ResourceState new_state,
        u32 base_mip_level, u32 mip_count, bool is_depth, u32 source_family, u32 destination_family,
        QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type) {

        if (gpu->gpu_device_features & GpuDeviceFeature_SYNCHRONIZATION2) {
            VkImageMemoryBarrier2KHR barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR };
            barrier.srcAccessMask = util_to_vk_access_flags2(old_state);
            barrier.srcStageMask = util_determine_pipeline_stage_flags2(barrier.srcAccessMask, source_queue_type);
            barrier.dstAccessMask = util_to_vk_access_flags2(new_state);
            barrier.dstStageMask = util_determine_pipeline_stage_flags2(barrier.dstAccessMask, destination_queue_type);
            barrier.oldLayout = util_to_vk_image_layout2(old_state);
            barrier.newLayout = util_to_vk_image_layout2(new_state);
            barrier.srcQueueFamilyIndex = source_family;
            barrier.dstQueueFamilyIndex = destination_family;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.subresourceRange.baseMipLevel = base_mip_level;
            barrier.subresourceRange.levelCount = mip_count;

            VkDependencyInfoKHR dependency_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR };
            dependency_info.imageMemoryBarrierCount = 1;
            dependency_info.pImageMemoryBarriers = &barrier;

            gpu->cmd_pipeline_barrier2(command_buffer, &dependency_info);
        } else{
            VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            barrier.image = image;
            barrier.srcQueueFamilyIndex = source_family;
            barrier.dstQueueFamilyIndex = destination_family;
            barrier.subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.subresourceRange.levelCount = mip_count;

            barrier.subresourceRange.baseMipLevel = base_mip_level;
            barrier.oldLayout = util_to_vk_image_layout(old_state);
            barrier.newLayout = util_to_vk_image_layout(new_state);
            barrier.srcAccessMask = util_to_vk_access_flags(old_state);
            barrier.dstAccessMask = util_to_vk_access_flags(new_state);

            const VkPipelineStageFlags source_stage_mask = util_determine_pipeline_stage_flags(barrier.srcAccessMask, source_queue_type);
            const VkPipelineStageFlags destination_stage_mask = util_determine_pipeline_stage_flags(barrier.dstAccessMask, destination_queue_type);

            vkCmdPipelineBarrier(command_buffer, source_stage_mask, destination_stage_mask, 0,
                0, nullptr, 0, nullptr, 1, &barrier);
        }
    }

    void util_add_buffer_barrier(GpuDevice* gpu, VkCommandBuffer command_buffer, VkBuffer buffer, ResourceState old_state, ResourceState new_state, u32 buffer_size) {

        util_add_buffer_barrier_ext(gpu, command_buffer, buffer, old_state, new_state, buffer_size,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, QueueType::Graphics, QueueType::Graphics);
    }

    void util_add_buffer_barrier_ext(GpuDevice* gpu, VkCommandBuffer command_buffer, VkBuffer buffer, ResourceState old_state, ResourceState new_state,
        u32 buffer_size, u32 source_family, u32 destination_family,
        QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type) {

        if (gpu->gpu_device_features & GpuDeviceFeature_SYNCHRONIZATION2) {
            VkBufferMemoryBarrier2KHR barrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR };
            barrier.srcAccessMask = util_to_vk_access_flags2(old_state);
            barrier.srcStageMask = util_determine_pipeline_stage_flags2(barrier.srcAccessMask, source_queue_type);
            barrier.dstAccessMask = util_to_vk_access_flags2(new_state);
            barrier.dstStageMask = util_determine_pipeline_stage_flags2(barrier.dstAccessMask, destination_queue_type);
            barrier.buffer = buffer;
            barrier.offset = 0;
            barrier.size = buffer_size;

            VkDependencyInfoKHR dependency_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR };
            dependency_info.bufferMemoryBarrierCount = 1;
            dependency_info.pBufferMemoryBarriers = &barrier;

            gpu->cmd_pipeline_barrier2(command_buffer, &dependency_info);
        } else{
            VkBufferMemoryBarrier barrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
            barrier.buffer = buffer;
            barrier.srcQueueFamilyIndex = source_family;
            barrier.dstQueueFamilyIndex = destination_family;
            barrier.offset = 0;
            barrier.size = buffer_size;
            barrier.srcAccessMask = util_to_vk_access_flags(old_state);
            barrier.dstAccessMask = util_to_vk_access_flags(new_state);

            const VkPipelineStageFlags source_stage_mask = util_determine_pipeline_stage_flags(barrier.srcAccessMask, source_queue_type);
            const VkPipelineStageFlags destination_stage_mask = util_determine_pipeline_stage_flags(barrier.dstAccessMask, destination_queue_type);

            vkCmdPipelineBarrier(command_buffer, source_stage_mask, destination_stage_mask, 0,
                    0, nullptr, 1, &barrier, 0, nullptr);
        }
    }

    VkFormat util_string_to_vk_format(cstring format) {
        if (strcmp(format, "VK_FORMAT_R4G4_UNORM_PACK8") == 0) {
            return VK_FORMAT_R4G4_UNORM_PACK8;
        }
        if (strcmp(format, "VK_FORMAT_R4G4B4A4_UNORM_PACK16") == 0) {
            return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
        }
        if (strcmp(format, "VK_FORMAT_B4G4R4A4_UNORM_PACK16") == 0) {
            return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
        }
        if (strcmp(format, "VK_FORMAT_R5G6B5_UNORM_PACK16") == 0) {
            return VK_FORMAT_R5G6B5_UNORM_PACK16;
        }
        if (strcmp(format, "VK_FORMAT_B5G6R5_UNORM_PACK16") == 0) {
            return VK_FORMAT_B5G6R5_UNORM_PACK16;
        }
        if (strcmp(format, "VK_FORMAT_R5G5B5A1_UNORM_PACK16") == 0) {
            return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
        }
        if (strcmp(format, "VK_FORMAT_B5G5R5A1_UNORM_PACK16") == 0) {
            return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
        }
        if (strcmp(format, "VK_FORMAT_A1R5G5B5_UNORM_PACK16") == 0) {
            return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
        }
        if (strcmp(format, "VK_FORMAT_R8_UNORM") == 0) {
            return VK_FORMAT_R8_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_R8_SNORM") == 0) {
            return VK_FORMAT_R8_SNORM;
        }
        if (strcmp(format, "VK_FORMAT_R8_USCALED") == 0) {
            return VK_FORMAT_R8_USCALED;
        }
        if (strcmp(format, "VK_FORMAT_R8_SSCALED") == 0) {
            return VK_FORMAT_R8_SSCALED;
        }
        if (strcmp(format, "VK_FORMAT_R8_UINT") == 0) {
            return VK_FORMAT_R8_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R8_SINT") == 0) {
            return VK_FORMAT_R8_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R8_SRGB") == 0) {
            return VK_FORMAT_R8_SRGB;
        }
        if (strcmp(format, "VK_FORMAT_R8G8_UNORM") == 0) {
            return VK_FORMAT_R8G8_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_R8G8_SNORM") == 0) {
            return VK_FORMAT_R8G8_SNORM;
        }
        if (strcmp(format, "VK_FORMAT_R8G8_USCALED") == 0) {
            return VK_FORMAT_R8G8_USCALED;
        }
        if (strcmp(format, "VK_FORMAT_R8G8_SSCALED") == 0) {
            return VK_FORMAT_R8G8_SSCALED;
        }
        if (strcmp(format, "VK_FORMAT_R8G8_UINT") == 0) {
            return VK_FORMAT_R8G8_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R8G8_SINT") == 0) {
            return VK_FORMAT_R8G8_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R8G8_SRGB") == 0) {
            return VK_FORMAT_R8G8_SRGB;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8_UNORM") == 0) {
            return VK_FORMAT_R8G8B8_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8_SNORM") == 0) {
            return VK_FORMAT_R8G8B8_SNORM;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8_USCALED") == 0) {
            return VK_FORMAT_R8G8B8_USCALED;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8_SSCALED") == 0) {
            return VK_FORMAT_R8G8B8_SSCALED;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8_UINT") == 0) {
            return VK_FORMAT_R8G8B8_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8_SINT") == 0) {
            return VK_FORMAT_R8G8B8_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8_SRGB") == 0) {
            return VK_FORMAT_R8G8B8_SRGB;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8_UNORM") == 0) {
            return VK_FORMAT_B8G8R8_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8_SNORM") == 0) {
            return VK_FORMAT_B8G8R8_SNORM;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8_USCALED") == 0) {
            return VK_FORMAT_B8G8R8_USCALED;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8_SSCALED") == 0) {
            return VK_FORMAT_B8G8R8_SSCALED;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8_UINT") == 0) {
            return VK_FORMAT_B8G8R8_UINT;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8_SINT") == 0) {
            return VK_FORMAT_B8G8R8_SINT;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8_SRGB") == 0) {
            return VK_FORMAT_B8G8R8_SRGB;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8A8_UNORM") == 0) {
            return VK_FORMAT_R8G8B8A8_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8A8_SNORM") == 0) {
            return VK_FORMAT_R8G8B8A8_SNORM;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8A8_USCALED") == 0) {
            return VK_FORMAT_R8G8B8A8_USCALED;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8A8_SSCALED") == 0) {
            return VK_FORMAT_R8G8B8A8_SSCALED;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8A8_UINT") == 0) {
            return VK_FORMAT_R8G8B8A8_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8A8_SINT") == 0) {
            return VK_FORMAT_R8G8B8A8_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R8G8B8A8_SRGB") == 0) {
            return VK_FORMAT_R8G8B8A8_SRGB;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8A8_UNORM") == 0) {
            return VK_FORMAT_B8G8R8A8_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8A8_SNORM") == 0) {
            return VK_FORMAT_B8G8R8A8_SNORM;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8A8_USCALED") == 0) {
            return VK_FORMAT_B8G8R8A8_USCALED;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8A8_SSCALED") == 0) {
            return VK_FORMAT_B8G8R8A8_SSCALED;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8A8_UINT") == 0) {
            return VK_FORMAT_B8G8R8A8_UINT;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8A8_SINT") == 0) {
            return VK_FORMAT_B8G8R8A8_SINT;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8A8_SRGB") == 0) {
            return VK_FORMAT_B8G8R8A8_SRGB;
        }
        if (strcmp(format, "VK_FORMAT_A8B8G8R8_UNORM_PACK32") == 0) {
            return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A8B8G8R8_SNORM_PACK32") == 0) {
            return VK_FORMAT_A8B8G8R8_SNORM_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A8B8G8R8_USCALED_PACK32") == 0) {
            return VK_FORMAT_A8B8G8R8_USCALED_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A8B8G8R8_SSCALED_PACK32") == 0) {
            return VK_FORMAT_A8B8G8R8_SSCALED_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A8B8G8R8_UINT_PACK32") == 0) {
            return VK_FORMAT_A8B8G8R8_UINT_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A8B8G8R8_SINT_PACK32") == 0) {
            return VK_FORMAT_A8B8G8R8_SINT_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A8B8G8R8_SRGB_PACK32") == 0) {
            return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2R10G10B10_UNORM_PACK32") == 0) {
            return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2R10G10B10_SNORM_PACK32") == 0) {
            return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2R10G10B10_USCALED_PACK32") == 0) {
            return VK_FORMAT_A2R10G10B10_USCALED_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2R10G10B10_SSCALED_PACK32") == 0) {
            return VK_FORMAT_A2R10G10B10_SSCALED_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2R10G10B10_UINT_PACK32") == 0) {
            return VK_FORMAT_A2R10G10B10_UINT_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2R10G10B10_SINT_PACK32") == 0) {
            return VK_FORMAT_A2R10G10B10_SINT_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2B10G10R10_UNORM_PACK32") == 0) {
            return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2B10G10R10_SNORM_PACK32") == 0) {
            return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2B10G10R10_USCALED_PACK32") == 0) {
            return VK_FORMAT_A2B10G10R10_USCALED_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2B10G10R10_SSCALED_PACK32") == 0) {
            return VK_FORMAT_A2B10G10R10_SSCALED_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2B10G10R10_UINT_PACK32") == 0) {
            return VK_FORMAT_A2B10G10R10_UINT_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_A2B10G10R10_SINT_PACK32") == 0) {
            return VK_FORMAT_A2B10G10R10_SINT_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_R16_UNORM") == 0) {
            return VK_FORMAT_R16_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_R16_SNORM") == 0) {
            return VK_FORMAT_R16_SNORM;
        }
        if (strcmp(format, "VK_FORMAT_R16_USCALED") == 0) {
            return VK_FORMAT_R16_USCALED;
        }
        if (strcmp(format, "VK_FORMAT_R16_SSCALED") == 0) {
            return VK_FORMAT_R16_SSCALED;
        }
        if (strcmp(format, "VK_FORMAT_R16_UINT") == 0) {
            return VK_FORMAT_R16_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R16_SINT") == 0) {
            return VK_FORMAT_R16_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R16_SFLOAT") == 0) {
            return VK_FORMAT_R16_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_R16G16_UNORM") == 0) {
            return VK_FORMAT_R16G16_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_R16G16_SNORM") == 0) {
            return VK_FORMAT_R16G16_SNORM;
        }
        if (strcmp(format, "VK_FORMAT_R16G16_USCALED") == 0) {
            return VK_FORMAT_R16G16_USCALED;
        }
        if (strcmp(format, "VK_FORMAT_R16G16_SSCALED") == 0) {
            return VK_FORMAT_R16G16_SSCALED;
        }
        if (strcmp(format, "VK_FORMAT_R16G16_UINT") == 0) {
            return VK_FORMAT_R16G16_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R16G16_SINT") == 0) {
            return VK_FORMAT_R16G16_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R16G16_SFLOAT") == 0) {
            return VK_FORMAT_R16G16_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16_UNORM") == 0) {
            return VK_FORMAT_R16G16B16_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16_SNORM") == 0) {
            return VK_FORMAT_R16G16B16_SNORM;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16_USCALED") == 0) {
            return VK_FORMAT_R16G16B16_USCALED;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16_SSCALED") == 0) {
            return VK_FORMAT_R16G16B16_SSCALED;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16_UINT") == 0) {
            return VK_FORMAT_R16G16B16_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16_SINT") == 0) {
            return VK_FORMAT_R16G16B16_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16_SFLOAT") == 0) {
            return VK_FORMAT_R16G16B16_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16A16_UNORM") == 0) {
            return VK_FORMAT_R16G16B16A16_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16A16_SNORM") == 0) {
            return VK_FORMAT_R16G16B16A16_SNORM;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16A16_USCALED") == 0) {
            return VK_FORMAT_R16G16B16A16_USCALED;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16A16_SSCALED") == 0) {
            return VK_FORMAT_R16G16B16A16_SSCALED;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16A16_UINT") == 0) {
            return VK_FORMAT_R16G16B16A16_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16A16_SINT") == 0) {
            return VK_FORMAT_R16G16B16A16_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R16G16B16A16_SFLOAT") == 0) {
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_R32_UINT") == 0) {
            return VK_FORMAT_R32_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R32_SINT") == 0) {
            return VK_FORMAT_R32_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R32_SFLOAT") == 0) {
            return VK_FORMAT_R32_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_R32G32_UINT") == 0) {
            return VK_FORMAT_R32G32_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R32G32_SINT") == 0) {
            return VK_FORMAT_R32G32_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R32G32_SFLOAT") == 0) {
            return VK_FORMAT_R32G32_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_R32G32B32_UINT") == 0) {
            return VK_FORMAT_R32G32B32_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R32G32B32_SINT") == 0) {
            return VK_FORMAT_R32G32B32_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R32G32B32_SFLOAT") == 0) {
            return VK_FORMAT_R32G32B32_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_R32G32B32A32_UINT") == 0) {
            return VK_FORMAT_R32G32B32A32_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R32G32B32A32_SINT") == 0) {
            return VK_FORMAT_R32G32B32A32_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R32G32B32A32_SFLOAT") == 0) {
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_R64_UINT") == 0) {
            return VK_FORMAT_R64_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R64_SINT") == 0) {
            return VK_FORMAT_R64_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R64_SFLOAT") == 0) {
            return VK_FORMAT_R64_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_R64G64_UINT") == 0) {
            return VK_FORMAT_R64G64_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R64G64_SINT") == 0) {
            return VK_FORMAT_R64G64_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R64G64_SFLOAT") == 0) {
            return VK_FORMAT_R64G64_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_R64G64B64_UINT") == 0) {
            return VK_FORMAT_R64G64B64_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R64G64B64_SINT") == 0) {
            return VK_FORMAT_R64G64B64_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R64G64B64_SFLOAT") == 0) {
            return VK_FORMAT_R64G64B64_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_R64G64B64A64_UINT") == 0) {
            return VK_FORMAT_R64G64B64A64_UINT;
        }
        if (strcmp(format, "VK_FORMAT_R64G64B64A64_SINT") == 0) {
            return VK_FORMAT_R64G64B64A64_SINT;
        }
        if (strcmp(format, "VK_FORMAT_R64G64B64A64_SFLOAT") == 0) {
            return VK_FORMAT_R64G64B64A64_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_B10G11R11_UFLOAT_PACK32") == 0) {
            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32") == 0) {
            return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_D16_UNORM") == 0) {
            return VK_FORMAT_D16_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_X8_D24_UNORM_PACK32") == 0) {
            return VK_FORMAT_X8_D24_UNORM_PACK32;
        }
        if (strcmp(format, "VK_FORMAT_D32_SFLOAT") == 0) {
            return VK_FORMAT_D32_SFLOAT;
        }
        if (strcmp(format, "VK_FORMAT_S8_UINT") == 0) {
            return VK_FORMAT_S8_UINT;
        }
        if (strcmp(format, "VK_FORMAT_D16_UNORM_S8_UINT") == 0) {
            return VK_FORMAT_D16_UNORM_S8_UINT;
        }
        if (strcmp(format, "VK_FORMAT_D24_UNORM_S8_UINT") == 0) {
            return VK_FORMAT_D24_UNORM_S8_UINT;
        }
        if (strcmp(format, "VK_FORMAT_D32_SFLOAT_S8_UINT") == 0) {
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        }
        if (strcmp(format, "VK_FORMAT_BC1_RGB_UNORM_BLOCK") == 0) {
            return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC1_RGB_SRGB_BLOCK") == 0) {
            return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC1_RGBA_UNORM_BLOCK") == 0) {
            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC1_RGBA_SRGB_BLOCK") == 0) {
            return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC2_UNORM_BLOCK") == 0) {
            return VK_FORMAT_BC2_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC2_SRGB_BLOCK") == 0) {
            return VK_FORMAT_BC2_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC3_UNORM_BLOCK") == 0) {
            return VK_FORMAT_BC3_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC3_SRGB_BLOCK") == 0) {
            return VK_FORMAT_BC3_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC4_UNORM_BLOCK") == 0) {
            return VK_FORMAT_BC4_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC4_SNORM_BLOCK") == 0) {
            return VK_FORMAT_BC4_SNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC5_UNORM_BLOCK") == 0) {
            return VK_FORMAT_BC5_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC5_SNORM_BLOCK") == 0) {
            return VK_FORMAT_BC5_SNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC6H_UFLOAT_BLOCK") == 0) {
            return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC6H_SFLOAT_BLOCK") == 0) {
            return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC7_UNORM_BLOCK") == 0) {
            return VK_FORMAT_BC7_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_BC7_SRGB_BLOCK") == 0) {
            return VK_FORMAT_BC7_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_EAC_R11_UNORM_BLOCK") == 0) {
            return VK_FORMAT_EAC_R11_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_EAC_R11_SNORM_BLOCK") == 0) {
            return VK_FORMAT_EAC_R11_SNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_EAC_R11G11_UNORM_BLOCK") == 0) {
            return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_EAC_R11G11_SNORM_BLOCK") == 0) {
            return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_4x4_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_4x4_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_5x4_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_5x4_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_5x5_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_5x5_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_6x5_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_6x5_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_6x6_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_6x6_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_8x5_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_8x5_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_8x6_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_8x6_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_8x8_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_8x8_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x5_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x5_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x6_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x6_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x8_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x8_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x10_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x10_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_12x10_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_12x10_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_12x12_UNORM_BLOCK") == 0) {
            return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_12x12_SRGB_BLOCK") == 0) {
            return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
        }
        if (strcmp(format, "VK_FORMAT_G8B8G8R8_422_UNORM") == 0) {
            return VK_FORMAT_G8B8G8R8_422_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_B8G8R8G8_422_UNORM") == 0) {
            return VK_FORMAT_B8G8R8G8_422_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM") == 0) {
            return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_G8_B8R8_2PLANE_420_UNORM") == 0) {
            return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM") == 0) {
            return VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_G8_B8R8_2PLANE_422_UNORM") == 0) {
            return VK_FORMAT_G8_B8R8_2PLANE_422_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM") == 0) {
            return VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_R10X6_UNORM_PACK16") == 0) {
            return VK_FORMAT_R10X6_UNORM_PACK16;
        }
        if (strcmp(format, "VK_FORMAT_R10X6G10X6_UNORM_2PACK16") == 0) {
            return VK_FORMAT_R10X6G10X6_UNORM_2PACK16;
        }
        if (strcmp(format, "VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16") == 0) {
            return VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16") == 0) {
            return VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16;
        }
        if (strcmp(format, "VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16") == 0) {
            return VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16") == 0) {
            return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16") == 0) {
            return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16") == 0) {
            return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16") == 0) {
            return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16") == 0) {
            return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16;
        }
        if (strcmp(format, "VK_FORMAT_R12X4_UNORM_PACK16") == 0) {
            return VK_FORMAT_R12X4_UNORM_PACK16;
        }
        if (strcmp(format, "VK_FORMAT_R12X4G12X4_UNORM_2PACK16") == 0) {
            return VK_FORMAT_R12X4G12X4_UNORM_2PACK16;
        }
        if (strcmp(format, "VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16") == 0) {
            return VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16") == 0) {
            return VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16;
        }
        if (strcmp(format, "VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16") == 0) {
            return VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16") == 0) {
            return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16") == 0) {
            return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16") == 0) {
            return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16") == 0) {
            return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16") == 0) {
            return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16;
        }
        if (strcmp(format, "VK_FORMAT_G16B16G16R16_422_UNORM") == 0) {
            return VK_FORMAT_G16B16G16R16_422_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_B16G16R16G16_422_UNORM") == 0) {
            return VK_FORMAT_B16G16R16G16_422_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM") == 0) {
            return VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_G16_B16R16_2PLANE_420_UNORM") == 0) {
            return VK_FORMAT_G16_B16R16_2PLANE_420_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM") == 0) {
            return VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_G16_B16R16_2PLANE_422_UNORM") == 0) {
            return VK_FORMAT_G16_B16R16_2PLANE_422_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM") == 0) {
            return VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM;
        }
        if (strcmp(format, "VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG") == 0) {
            return VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;
        }
        if (strcmp(format, "VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG") == 0) {
            return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
        }
        if (strcmp(format, "VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG") == 0) {
            return VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG;
        }
        if (strcmp(format, "VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG") == 0) {
            return VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG;
        }
        if (strcmp(format, "VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG") == 0) {
            return VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG;
        }
        if (strcmp(format, "VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG") == 0) {
            return VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG;
        }
        if (strcmp(format, "VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG") == 0) {
            return VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG;
        }
        if (strcmp(format, "VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG") == 0) {
            return VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT") == 0) {
            return VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT;
        }
        if (strcmp(format, "VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT") == 0) {
            return VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT;
        }
        if (strcmp(format, "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT") == 0) {
            return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT;
        }
        if (strcmp(format, "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT") == 0) {
            return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT;
        }
        if (strcmp(format, "VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT") == 0) {
            return VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT;
        }
        if (strcmp(format, "VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT") == 0) {
            return VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT;
        }
        if (strcmp(format, "VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT") == 0) {
            return VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT;
        }

        HASSERT_MSG(false, "Invalid VkFormat string");
        return VK_FORMAT_UNDEFINED;
    }

    //
    //
    VkAccessFlags util_to_vk_access_flags2(ResourceState state) {
        VkAccessFlags ret = 0;
        if (state & RESOURCE_STATE_COPY_SOURCE) {
            ret |= VK_ACCESS_2_TRANSFER_READ_BIT_KHR;
        }
        if (state & RESOURCE_STATE_COPY_DEST) {
            ret |= VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
        }
        if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) {
            ret |= VK_ACCESS_2_UNIFORM_READ_BIT_KHR | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR;
        }
        if (state & RESOURCE_STATE_INDEX_BUFFER) {
            ret |= VK_ACCESS_2_INDEX_READ_BIT_KHR;
        }
        if (state & RESOURCE_STATE_UNORDERED_ACCESS) {
            ret |= VK_ACCESS_2_SHADER_READ_BIT_KHR | VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
        }
        if (state & RESOURCE_STATE_INDIRECT_ARGUMENT) {
            ret |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR;
        }
        if (state & RESOURCE_STATE_RENDER_TARGET) {
            ret |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
        }
        if (state & RESOURCE_STATE_DEPTH_WRITE) {
            ret |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR;
        }
        if (state & RESOURCE_STATE_SHADER_RESOURCE) {
            ret |= VK_ACCESS_2_SHADER_READ_BIT_KHR;
        }
        if (state & RESOURCE_STATE_PRESENT) {
            ret |= VK_ACCESS_2_MEMORY_READ_BIT_KHR;
        }
#ifdef ENABLE_RAYTRACING
        if (state & RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) {
            ret |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
        }
#endif

        return ret;
    }

    //
    //
    static VkImageLayout util_to_vk_image_layout(ResourceState usage) {
        if (usage & RESOURCE_STATE_COPY_SOURCE)
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        if (usage & RESOURCE_STATE_COPY_DEST)
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        if (usage & RESOURCE_STATE_RENDER_TARGET)
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        if (usage & RESOURCE_STATE_DEPTH_WRITE)
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        if (usage & RESOURCE_STATE_DEPTH_READ)
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        if (usage & RESOURCE_STATE_UNORDERED_ACCESS)
            return VK_IMAGE_LAYOUT_GENERAL;

        if (usage & RESOURCE_STATE_SHADER_RESOURCE)
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        if (usage & RESOURCE_STATE_PRESENT)
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        if (usage == RESOURCE_STATE_COMMON)
            return VK_IMAGE_LAYOUT_GENERAL;

        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    //
    //
    VkImageLayout util_to_vk_image_layout2(ResourceState usage) {
        if (usage & RESOURCE_STATE_COPY_SOURCE)
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        if (usage & RESOURCE_STATE_COPY_DEST)
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        if (usage & RESOURCE_STATE_RENDER_TARGET)
            return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;

        if (usage & RESOURCE_STATE_DEPTH_WRITE)
            return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;

        if (usage & RESOURCE_STATE_DEPTH_READ)
            return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;

        if (usage & RESOURCE_STATE_UNORDERED_ACCESS)
            return VK_IMAGE_LAYOUT_GENERAL;

        if (usage & RESOURCE_STATE_SHADER_RESOURCE)
            return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;

        if (usage & RESOURCE_STATE_PRESENT)
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        if (usage == RESOURCE_STATE_COMMON)
            return VK_IMAGE_LAYOUT_GENERAL;

        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    //
    //
    static VkPipelineStageFlags util_determine_pipeline_stage_flags(VkAccessFlags accessFlags, QueueType::Enum queueType) {
        VkPipelineStageFlags flags = 0;

        switch (queueType) {
        case QueueType::Graphics:
        {
            if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
                flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

            if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0) {
                flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
                flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                /*if ( pRenderer->pActiveGpuSettings->mGeometryShaderSupported ) {
                    flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
                }
                if ( pRenderer->pActiveGpuSettings->mTessellationSupported ) {
                    flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
                    flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
                }*/
                flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
#ifdef ENABLE_RAYTRACING
                if (pRenderer->mVulkan.mRaytracingExtension) {
                    flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
                }
#endif
            }

            if ((accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
                flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

            if ((accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
                flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            if ((accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
                flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

            break;
        }
        case QueueType::Compute:
        {
            if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
                (accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
                (accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
                (accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
                return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
                flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            break;
        }
        case QueueType::CopyTransfer: return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        default: break;
        }

        // Compatible with both compute and graphics queues
        if ((accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
            flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

        if ((accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

        if ((accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_HOST_BIT;

        if (flags == 0)
            flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        return flags;
    }

    VkPipelineStageFlags2KHR util_determine_pipeline_stage_flags2(VkAccessFlags2KHR access_flags, QueueType::Enum queue_type) {
        VkPipelineStageFlags2KHR flags = 0;

        switch (queue_type) {
        case QueueType::Graphics:
        {
            if ((access_flags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
                flags |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR;

            if ((access_flags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0) {
                flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
                flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
                /*if ( pRenderer->pActiveGpuSettings->mGeometryShaderSupported ) {
                    flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
                }
                if ( pRenderer->pActiveGpuSettings->mTessellationSupported ) {
                    flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
                    flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
                }*/
                flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
#ifdef ENABLE_RAYTRACING
                if (pRenderer->mVulkan.mRaytracingExtension) {
                    flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
                }
#endif
            }

            if ((access_flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
                flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;

            if ((access_flags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
                flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

            if ((access_flags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
                flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR;

            break;
        }
        case QueueType::Compute:
        {
            if ((access_flags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
                (access_flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
                (access_flags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
                (access_flags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
                return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;

            if ((access_flags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
                flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;

            break;
        }
        case QueueType::CopyTransfer: return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
        default: break;
        }

        // Compatible with both compute and graphics queues
        if ((access_flags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
            flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR;

        if ((access_flags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;

        if ((access_flags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_2_HOST_BIT_KHR;

        if (flags == 0)
            flags = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR;

        return flags;
    }

} // namespace Helix
