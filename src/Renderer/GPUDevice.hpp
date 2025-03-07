#pragma once

#if (_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include <vulkan/vulkan.h>

#include "vendor/vk_mem_alloc.h"

#include "Renderer/GPUResources.hpp"

#include "Core/DataStructures.hpp"
#include "Core/String.hpp"
#include "Core/Service.hpp"
#include "Core/Array.hpp"

#define NVIDIA 0

namespace Helix {

    struct Allocator;

    // Forward-declarations //////////////////////////////////////////////////
    struct CommandBuffer;
    struct DeviceRenderFrame;
    struct GPUTimestampManager;
    struct GpuDevice;


    //
    //
    struct GPUTimestamp {

        u32                             start;
        u32                             end;

        f64                             elapsed_ms;

        u16                             parent_index;
        u16                             depth;

        u32                             color;
        u32                             frame_index;

        cstring name;
    }; // struct GPUTimestamp


    struct GPUTimestampManager {

        void                            init(Allocator* allocator, u16 queries_per_frame, u16 max_frames);
        void                            shutdown();

        bool                            has_valid_queries() const;
        void                            reset();
        u32                             resolve(u32 current_frame, GPUTimestamp* timestamps_to_fill);    // Returns the total queries for this frame.

        u32                             push(u32 current_frame, cstring name);    // Returns the timestamp query index.
        u32                             pop(u32 current_frame);

        Allocator*                      allocator = nullptr;
        GPUTimestamp*                   timestamps = nullptr;
        u64*                            timestamps_data = nullptr;

        u32                             queries_per_frame = 0;
        u32                             current_query = 0;
        u32                             parent_index = 0;
        u32                             depth = 0;

        bool                            current_frame_resolved = false;    // Used to query the GPU only once per frame if get_gpu_timestamps is called more than once per frame.

    }; // struct GPUTimestampManager

    static const uint32_t               k_max_frames = 3;

    //
    //
    struct DeviceCreation {

        Allocator*                      allocator                   = nullptr;
        StackAllocator*                 temporary_allocator         = nullptr;
        void*                           window                      = nullptr; // Pointer to API-specific window: SDL_Window, GLFWWindow
        u16                             width                       = 1;
        u16                             height                      = 1;

        u16                             gpu_time_queries_per_frame  = 32;
        u16                             num_threads                 = 1;
        bool                            enable_gpu_time_queries     = false;
        bool                            debug                       = false;

        DeviceCreation&                 set_window(u32 width, u32 height, void* handle);
        DeviceCreation&                 set_allocator(Allocator* allocator);
        DeviceCreation&                 set_linear_allocator(StackAllocator* allocator);
        DeviceCreation&                 set_num_threads(u32 value);

    }; // struct DeviceCreation

    enum GpuDeviceFeature : u8 {
        GpuDeviceFeature_BINDLESS = 1 << 0,
        GpuDeviceFeature_DYNAMIC_RENDERING = 1 << 1,
        GpuDeviceFeature_TIMELINE_SEMAPHORE = 1 << 2,
        GpuDeviceFeature_SYNCHRONIZATION2 = 1 << 3,
        GpuDeviceFeature_MESH_SHADER = 1 << 4,

    };
    inline GpuDeviceFeature operator|(GpuDeviceFeature a, GpuDeviceFeature b) {
        return static_cast<GpuDeviceFeature>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline GpuDeviceFeature operator|=(GpuDeviceFeature& a, GpuDeviceFeature b) {
        a = a | b;
        return a;
    }

    inline GpuDeviceFeature operator&(GpuDeviceFeature a, GpuDeviceFeature b) {
        return static_cast<GpuDeviceFeature>(static_cast<int>(a) & static_cast<int>(b));
    }
    inline GpuDeviceFeature operator&=(GpuDeviceFeature& a, GpuDeviceFeature b) {
        a = a & b;
        return a;
    }

    //
    //
    struct GpuDevice : public Service {

        static GpuDevice*               instance();

        // Helper methods
        static void                     fill_write_descriptor_sets(GpuDevice& gpu, const DesciptorSetLayout* descriptor_set_layout, VkDescriptorSet vk_descriptor_set,
            VkWriteDescriptorSet* descriptor_write, VkDescriptorBufferInfo* buffer_info, VkDescriptorImageInfo* image_info,
            VkSampler vk_default_sampler, u32& num_resources, const ResourceHandle* resources, const SamplerHandle* samplers, const u16* bindings);

        // Init/Terminate methods
        void                            init(const DeviceCreation& creation);
        void                            shutdown();

        // Creation/Destruction of resources /////////////////////////////////
        BufferHandle                    create_buffer(const BufferCreation& creation);
        TextureHandle                   create_texture(const TextureCreation& creation);
        TextureHandle                   create_texture_view(const TextureViewCreation& creation);
        PipelineHandle                  create_pipeline(const PipelineCreation& creation, cstring cache_path = nullptr);
        SamplerHandle                   create_sampler(const SamplerCreation& creation);
        DescriptorSetLayoutHandle       create_descriptor_set_layout(const DescriptorSetLayoutCreation& creation);
        DescriptorSetHandle             create_descriptor_set(const DescriptorSetCreation& creation);
        RenderPassHandle                create_render_pass(const RenderPassCreation& creation);
        FramebufferHandle               create_framebuffer(const FramebufferCreation& creation);
        ShaderStateHandle               create_shader_state(const ShaderStateCreation& creation);

        void                            destroy_buffer(BufferHandle buffer);
        void                            destroy_texture(TextureHandle texture);
        void                            destroy_pipeline(PipelineHandle pipeline);
        void                            destroy_sampler(SamplerHandle sampler);
        void                            destroy_descriptor_set_layout(DescriptorSetLayoutHandle layout);
        void                            destroy_descriptor_set(DescriptorSetHandle set);
        void                            destroy_render_pass(RenderPassHandle render_pass);
        void                            destroy_framebuffer(FramebufferHandle framebuffer);
        void                            destroy_shader_state(ShaderStateHandle shader);

        // Query Description /////////////////////////////////////////////////
        void                            query_buffer(BufferHandle handle, BufferDescription& out_description);
        void                            query_texture(TextureHandle handle, TextureDescription& out_description);
        void                            query_pipeline(PipelineHandle handle, PipelineDescription& out_description);
        void                            query_sampler(SamplerHandle handle, SamplerDescription& out_description);
        void                            query_descriptor_set_layout(DescriptorSetLayoutHandle handle, DescriptorSetLayoutDescription& out_description);
        void                            query_descriptor_set(DescriptorSetHandle handle, DesciptorSetDescription& out_description);
        void                            query_shader_state(ShaderStateHandle handle, ShaderStateDescription& out_description);

        const RenderPassOutput&         get_render_pass_output(RenderPassHandle render_pass) const;

        // Update/Reload resources ///////////////////////////////////////////
        void                            resize_output_textures(FramebufferHandle framebuffer, u32 width, u32 height);
        void                            resize_texture(TextureHandle texture, u32 width, u32 height);

        void                            update_descriptor_set(DescriptorSetHandle set);

        // Misc //////////////////////////////////////////////////////////////
        void                            link_texture_sampler(TextureHandle texture, SamplerHandle sampler);   // TODO: for now specify a sampler for a texture or use the default one.

        void                            set_present_mode(PresentMode::Enum mode);

        void                            frame_counters_advance();

        bool                            get_family_queue(VkPhysicalDevice physical_device);

        VkShaderModuleCreateInfo        compile_shader(cstring code, u32 code_size, VkShaderStageFlagBits stage, cstring name);

        // Swapchain //////////////////////////////////////////////////////////
        void                            create_swapchain();
        void                            destroy_swapchain();
        void                            resize_swapchain();

        // Map/Unmap /////////////////////////////////////////////////////////
        void*                           map_buffer(const MapBufferParameters& parameters);
        void                            unmap_buffer(const MapBufferParameters& parameters);

        void*                           dynamic_allocate(u32 size);

        void                            set_buffer_global_offset(BufferHandle buffer, u32 offset);

        // Command Buffers ///////////////////////////////////////////////////
        CommandBuffer*                  get_command_buffer(u32 thread_index, bool begin);
        CommandBuffer*                  get_secondary_command_buffer(u32 thread_index);

        void                            queue_command_buffer(CommandBuffer* command_buffer);          // Queue command buffer that will not be executed until present is called.

        // Rendering /////////////////////////////////////////////////////////
        void                            new_frame();
        void                            present(CommandBuffer* async_compute_command_buffer);
        void                            resize(u16 width, u16 height);
        void                            set_presentation_mode(PresentMode::Enum mode);

        void                            fill_barrier(RenderPassHandle render_pass, ExecutionBarrier& out_barrier);

        BufferHandle                    get_fullscreen_vertex_buffer() const;           // Returns a vertex buffer usable for fullscreen shaders that uses no vertices.
        RenderPassHandle                get_swapchain_pass() const;                     // Returns what is considered the final pass that writes to the swapchain.
        FramebufferHandle               get_current_framebuffer() const;                // Returns the framebuffer for the active swapchain image

        TextureHandle                   get_dummy_texture() const;
        BufferHandle                    get_dummy_constant_buffer() const;
        const RenderPassOutput&         get_swapchain_output() const { return swapchain_pass_output; }

        VkRenderPass                    get_vulkan_render_pass(const RenderPassOutput& output, cstring name);

        // Compute ///////////////////////////////////////////////////////////
        void                            submit_compute_load(CommandBuffer* command_buffer);

        // Names and markers /////////////////////////////////////////////////
        void                            set_resource_name(VkObjectType object_type, uint64_t handle, cstring name);
        void                            push_marker(VkCommandBuffer command_buffer, cstring name);
        void                            pop_marker(VkCommandBuffer command_buffer);

        // GPU Timings ///////////////////////////////////////////////////////
        void                            set_gpu_timestamps_enable(bool value) { timestamps_enabled = value; }

        u32                             get_gpu_timestamps(GPUTimestamp* out_timestamps);
        void                            push_gpu_timestamp(CommandBuffer* command_buffer, cstring name);
        void                            pop_gpu_timestamp(CommandBuffer* command_buffer);


        // Instant methods ///////////////////////////////////////////////////
        void                            destroy_buffer_instant(ResourceHandle buffer);
        void                            destroy_texture_instant(ResourceHandle texture);
        void                            destroy_pipeline_instant(ResourceHandle pipeline);
        void                            destroy_sampler_instant(ResourceHandle sampler);
        void                            destroy_descriptor_set_layout_instant(ResourceHandle layout);
        void                            destroy_descriptor_set_instant(ResourceHandle set);
        void                            destroy_render_pass_instant(ResourceHandle render_pass);
        void                            destroy_framebuffer_instant(ResourceHandle framebuffer);
        void                            destroy_shader_state_instant(ResourceHandle shader);

        void                            update_descriptor_set_instant(const DescriptorSetUpdate& update);

        // Memory Statistics //////////////////////////////////////////////////
        u32                             get_memory_heap_count();

        ResourcePool                    buffers;
        ResourcePool                    textures;
        ResourcePool                    pipelines;
        ResourcePool                    samplers;
        ResourcePool                    descriptor_set_layouts;
        ResourcePool                    descriptor_sets;
        ResourcePool                    render_passes;
        ResourcePool                    framebuffers;
        ResourcePool                    command_buffers;
        ResourcePool                    shaders;

        // Primitive resources
        BufferHandle                    fullscreen_vertex_buffer;
        TextureHandle                   fullscreen_texture_handle;
        RenderPassHandle                fullscreen_render_pass{ k_invalid_index };
        FramebufferHandle               fullscreen_framebuffer{ k_invalid_index };
        RenderPassHandle                swapchain_render_pass{ k_invalid_index };
        SamplerHandle                   default_sampler;
        // Dummy resources
        // TODO: Remove the word "Dummy"
        u32                             global_offset = 0;
        TextureHandle                   default_texture;
        BufferHandle                    default_constant_buffer;

        RenderPassOutput                fullscreen_pass_output;//
        RenderPassOutput                swapchain_pass_output; // TODO: Right now these are not used becuase RenderPassOutputs are generated.

        StringBuffer                    string_buffer;

        Allocator* allocator;
        StackAllocator* temporary_allocator;

        u32                             dynamic_max_per_frame_size;
        BufferHandle                    dynamic_buffer;
        u8*                             dynamic_mapped_memory;
        u32                             dynamic_allocated_size;
        u32                             dynamic_per_frame_size;

        CommandBuffer**                 queued_command_buffers = nullptr;
        u32                             num_allocated_command_buffers = 0;
        u32                             num_queued_command_buffers = 0;

        PresentMode::Enum               present_mode = PresentMode::Immediate;
        u32                             current_frame;
        u32                             previous_frame;

        u32                             absolute_frame;

        u16                             swapchain_width = 1;
        u16                             swapchain_height = 1;

        GPUTimestampManager*            gpu_timestamp_manager = nullptr;

        GpuDeviceFeature                gpu_device_features;

        bool                            timestamps_enabled = false;
        bool                            resized = false;
        bool                            vertical_sync = false;

        static constexpr cstring        k_name = "helix_gpu_service";


        VkAllocationCallbacks*          vulkan_allocation_callbacks;
        VkInstance                      vulkan_instance;
        VkPhysicalDevice                vulkan_physical_device;
        VkPhysicalDeviceProperties      vulkan_physical_properties;
        VkDevice                        vulkan_device;
        VkQueue                         vulkan_main_queue;
        VkQueue                         vulkan_compute_queue;
        VkQueue                         vulkan_transfer_queue;
        u32                             vulkan_main_queue_family;
        u32                             vulkan_compute_queue_family;
        u32                             vulkan_transfer_queue_family;
        VkDescriptorPool                vulkan_descriptor_pool;

        // [TAG: BINDLESS]
        VkDescriptorPool                vulkan_bindless_descriptor_pool;
        VkDescriptorSetLayout           vulkan_bindless_descriptor_layout;      // Global bindless descriptor layout.
        VkDescriptorSet                 vulkan_bindless_descriptor_set;         // Global bindless descriptor set.

        // Swapchain
        FramebufferHandle               vulkan_swapchain_framebuffers[k_max_swapchain_images]{ k_invalid_index, k_invalid_index, k_invalid_index };

        VkQueryPool                     vulkan_timestamp_query_pool;
        // Per frame synchronization
        VkSemaphore                     vulkan_render_complete_semaphore[k_max_swapchain_images];
        VkSemaphore                     vulkan_image_acquired_semaphore[k_max_swapchain_images];
        VkSemaphore                     vulkan_timeline_graphics_semaphore;
        VkFence                         vulkan_command_buffer_executed_fence[k_max_swapchain_images];

        VkSemaphore                     vulkan_compute_semaphore;
        VkFence                         vulkan_compute_fence;
        u64                             last_compute_semaphore_value = 0;
        bool                            has_async_work = false;


        // Windows specific
        VkSurfaceKHR                    vulkan_window_surface;
        VkSurfaceFormatKHR              vulkan_surface_format;
        VkPresentModeKHR                vulkan_present_mode;
        VkSwapchainKHR                  vulkan_swapchain;
        u32                             vulkan_swapchain_image_count;

        VkDebugReportCallbackEXT        vulkan_debug_callback;
        VkDebugUtilsMessengerEXT        vulkan_debug_utils_messenger;

        u32                             vulkan_image_index;

        VmaAllocator                    vma_allocator;

        // Extension functions
        PFN_vkCmdBeginRenderingKHR      cmd_begin_rendering;
        PFN_vkCmdEndRenderingKHR        cmd_end_rendering;
        PFN_vkQueueSubmit2KHR           queue_submit2;
        PFN_vkCmdPipelineBarrier2KHR    cmd_pipeline_barrier2;

#if NVIDIA
        // Mesh shaders functions
        PFN_vkCmdDrawMeshTasksNV        cmd_draw_mesh_tasks;
        PFN_vkCmdDrawMeshTasksIndirectCountNV cmd_draw_mesh_tasks_indirect_count;
        PFN_vkCmdDrawMeshTasksIndirectNV cmd_draw_mesh_tasks_indirect;
#else
        // Mesh shaders functions
        PFN_vkCmdDrawMeshTasksEXT        cmd_draw_mesh_tasks;
        PFN_vkCmdDrawMeshTasksIndirectCountEXT cmd_draw_mesh_tasks_indirect_count;
        PFN_vkCmdDrawMeshTasksIndirectEXT cmd_draw_mesh_tasks_indirect;
#endif // NVIDIA

        // These are dynamic - so that workload can be handled correctly.
        Array<ResourceUpdate>           resource_deletion_queue;
        Array<DescriptorSetUpdate>      descriptor_set_updates;
        // [TAG: BINDLESS]
        Array<ResourceUpdate>           texture_to_update_bindless;

        f32                             gpu_timestamp_frequency;
        bool                            gpu_timestamp_reset = true;
        bool                            debug_utils_extension_present = false; // Maybe

        char                            vulkan_binaries_path[512];


        ShaderState*                    access_shader_state(ShaderStateHandle shader);
        const ShaderState*              access_shader_state(ShaderStateHandle shader) const;

        Texture*                        access_texture(TextureHandle texture);
        const Texture*                  access_texture(TextureHandle texture) const;

        Buffer*                         access_buffer(BufferHandle buffer);
        const Buffer*                   access_buffer(BufferHandle buffer) const;

        Pipeline*                       access_pipeline(PipelineHandle pipeline);
        const Pipeline*                 access_pipeline(PipelineHandle pipeline) const;

        Sampler*                        access_sampler(SamplerHandle sampler);
        const Sampler*                  access_sampler(SamplerHandle sampler) const;

        DesciptorSetLayout*             access_descriptor_set_layout(DescriptorSetLayoutHandle layout);
        const DesciptorSetLayout*       access_descriptor_set_layout(DescriptorSetLayoutHandle layout) const;

        DescriptorSetLayoutHandle       get_descriptor_set_layout(PipelineHandle pipeline_handle, int layout_index);
        DescriptorSetLayoutHandle       get_descriptor_set_layout(PipelineHandle pipeline_handle, int layout_index) const;

        DescriptorSet*                  access_descriptor_set(DescriptorSetHandle set);
        const DescriptorSet*            access_descriptor_set(DescriptorSetHandle set) const;

        RenderPass*                     access_render_pass(RenderPassHandle render_pass);
        const RenderPass*               access_render_pass(RenderPassHandle render_pass) const;

        Framebuffer*                    access_framebuffer(FramebufferHandle framebuffer);
        const Framebuffer*              access_framebuffer(FramebufferHandle framebuffer) const;
    }; // struct Device


} // namespace Helix
