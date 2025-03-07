#include "Renderer/AsynchronousLoader.hpp"
#include "Renderer/Renderer.hpp"

#include "Core/Time.hpp"

#include "vendor/imgui/stb_image.h"

#include "vendor/tracy/tracy/Tracy.hpp"

namespace Helix
{
    // AsynchonousLoader //////////////////////////////////////////////////////

    void AsynchronousLoader::init(Renderer* renderer_, enki::TaskScheduler* task_scheduler_, Allocator* resident_allocator) {
        renderer = renderer_;
        task_scheduler = task_scheduler_;
        allocator = resident_allocator;

        file_load_requests.init(allocator, 16);
        upload_requests.init(allocator, 16);

        texture_ready.index = k_invalid_texture.index;
        cpu_buffer_ready.index = k_invalid_buffer.index;
        gpu_buffer_ready.index = k_invalid_buffer.index;
        completed = nullptr;

        using namespace Helix;

        // Create a persistently-mapped staging buffer
        BufferCreation bc;
        bc.reset().set(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ResourceUsageType::Stream, hmega(64)).set_name("staging_buffer").set_persistent(true);
        BufferHandle staging_buffer_handle = renderer->gpu->create_buffer(bc);

        staging_buffer = renderer->gpu->access_buffer(staging_buffer_handle);

        staging_buffer_offset = 0;

        for (u32 i = 0; i < k_max_frames; ++i) {
            VkCommandPoolCreateInfo cmd_pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
            cmd_pool_info.queueFamilyIndex = renderer->gpu->vulkan_transfer_queue_family;
            cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            vkCreateCommandPool(renderer->gpu->vulkan_device, &cmd_pool_info, renderer->gpu->vulkan_allocation_callbacks, &command_pools[i]);

            VkCommandBufferAllocateInfo cmd = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
            cmd.commandPool = command_pools[i];
            cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmd.commandBufferCount = 1;

            vkAllocateCommandBuffers(renderer->gpu->vulkan_device, &cmd, &command_buffers[i].vk_handle);

            command_buffers[i].is_recording = false;
            command_buffers[i].device = renderer->gpu;
        }

        VkSemaphoreCreateInfo semaphore_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        vkCreateSemaphore(renderer->gpu->vulkan_device, &semaphore_info, renderer->gpu->vulkan_allocation_callbacks, &transfer_complete_semaphore);

        VkFenceCreateInfo fence_info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(renderer->gpu->vulkan_device, &fence_info, renderer->gpu->vulkan_allocation_callbacks, &transfer_fence);
    }

    void AsynchronousLoader::shutdown() {

        renderer->gpu->destroy_buffer(staging_buffer->handle);

        file_load_requests.shutdown();
        upload_requests.shutdown();

        for (u32 i = 0; i < k_max_frames; ++i) {
            vkDestroyCommandPool(renderer->gpu->vulkan_device, command_pools[i], renderer->gpu->vulkan_allocation_callbacks);
            // Command buffers are destroyed with the pool associated.
        }

        vkDestroySemaphore(renderer->gpu->vulkan_device, transfer_complete_semaphore, renderer->gpu->vulkan_allocation_callbacks);
        vkDestroyFence(renderer->gpu->vulkan_device, transfer_fence, renderer->gpu->vulkan_allocation_callbacks);
    }

    void AsynchronousLoader::update(Allocator* stack_allocator) {

        // If a texture was processed in the previous commands, signal the renderer
        if (texture_ready.index != k_invalid_texture.index) {
            // Add update request.
            // This method is multithreaded_safe
            renderer->add_texture_to_update(texture_ready);
            texture_ready = k_invalid_texture;
        }

        if (cpu_buffer_ready.index != k_invalid_buffer.index) {
            HASSERT(completed != nullptr);
            (*completed)++;

            // TODO(marco): free cpu buffer

            gpu_buffer_ready.index = k_invalid_buffer.index;
            cpu_buffer_ready.index = k_invalid_buffer.index;
            completed = nullptr;
        }


        // Process upload requests
        if (upload_requests.size) {
            ZoneScoped;
            // TODO: Maybe have fences for each frame
            // Wait for transfer fence to be finished
            if (vkGetFenceStatus(renderer->gpu->vulkan_device, transfer_fence) != VK_SUCCESS) {
                return;
            }
            // Reset if file requests are present.
            vkResetFences(renderer->gpu->vulkan_device, 1, &transfer_fence);

            // Get last request
            UploadRequest request = upload_requests.back();
            upload_requests.pop();

            CommandBuffer* cb = &command_buffers[renderer->gpu->current_frame];
            cb->begin();

            if (request.texture.index != k_invalid_texture.index) {
                Texture* texture = renderer->gpu->access_texture(request.texture);
                const u32 k_texture_channels = 4;
                const u32 k_texture_alignment = 4;
                const sizet aligned_image_size = memory_align(texture->width * texture->height * k_texture_channels, k_texture_alignment);
                // Request place in buffer
                const sizet current_offset = std::atomic_fetch_add(&staging_buffer_offset, aligned_image_size);

                cb->upload_texture_data(texture->handle, request.data, staging_buffer->handle, current_offset);

                free(request.data);
            }
            else if (request.cpu_buffer.index != k_invalid_buffer.index && request.gpu_buffer.index != k_invalid_buffer.index) {
                Buffer* src = renderer->gpu->access_buffer(request.cpu_buffer);
                Buffer* dst = renderer->gpu->access_buffer(request.gpu_buffer);

                cb->upload_buffer_data(src->handle, dst->handle);
            }
            else if (request.cpu_buffer.index != k_invalid_buffer.index) {
                Buffer* buffer = renderer->gpu->access_buffer(request.cpu_buffer);
                // TODO: proper alignment
                const sizet aligned_image_size = memory_align(buffer->size, 64);
                const sizet current_offset = std::atomic_fetch_add(&staging_buffer_offset, aligned_image_size);
                cb->upload_buffer_data(buffer->handle, request.data, staging_buffer->handle, current_offset);

                free(request.data);
            }
            else if (request.gpu_buffer.index != k_invalid_buffer.index) {
                Buffer* buffer = renderer->gpu->access_buffer(request.gpu_buffer);
                const sizet current_offset = std::atomic_fetch_add(&staging_buffer_offset, buffer->size);
                cb->upload_buffer_data(buffer->handle, request.data, staging_buffer->handle, current_offset);
            }

            cb->end();

            VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cb->vk_handle;
            VkPipelineStageFlags wait_flag[]{ VK_PIPELINE_STAGE_TRANSFER_BIT };
            VkSemaphore wait_semaphore[]{ transfer_complete_semaphore };
            submitInfo.pWaitSemaphores = wait_semaphore;
            submitInfo.pWaitDstStageMask = wait_flag;

            VkQueue used_queue = renderer->gpu->vulkan_transfer_queue;
            vkQueueSubmit(used_queue, 1, &submitInfo, transfer_fence);

            // TODO(marco): better management for state machine. We need to account for file -> buffer,
            // buffer -> texture and buffer -> buffer. One the CPU buffer has been used it should be freed.
            if (request.texture.index != k_invalid_index) {
                HASSERT(texture_ready.index == k_invalid_texture.index);
                texture_ready = request.texture;
            }
            else if (request.cpu_buffer.index != k_invalid_buffer.index && request.gpu_buffer.index != k_invalid_buffer.index) {
                HASSERT(cpu_buffer_ready.index == k_invalid_index);
                HASSERT(gpu_buffer_ready.index == k_invalid_index);
                HASSERT(completed == nullptr);
                cpu_buffer_ready = request.cpu_buffer;
                gpu_buffer_ready = request.gpu_buffer;
                completed = request.completed;
            }
            else if (request.cpu_buffer.index != k_invalid_index) {
                HASSERT(cpu_buffer_ready.index == k_invalid_index);
                cpu_buffer_ready = request.cpu_buffer;
            }
            staging_buffer_offset = 0;
        }

        // Process a file request
        if (file_load_requests.size) {
            FileLoadRequest& load_request = file_load_requests.back();
            file_load_requests.pop();

            i64 start_reading_file = Time::now();
            // Process request
            int x, y, comp;
            u8* texture_data = stbi_load(load_request.path, &x, &y, &comp, 4);

            if (texture_data) {
                HINFO("File {} read in {} ms", load_request.path, Time::from_milliseconds(start_reading_file));

                UploadRequest& upload_request = upload_requests.push_use();
                upload_request.data = texture_data;
                upload_request.texture = load_request.texture;
                upload_request.cpu_buffer = k_invalid_buffer;
            }
            else {// TODO: use defualt texture if none found
                HCRITICAL("Error reading file {}", load_request.path);
            }
        }

    }

    void AsynchronousLoader::request_texture_data(cstring filename, TextureHandle texture) {

        FileLoadRequest& request = file_load_requests.push_use();
        strcpy(request.path, filename);
        request.texture = texture;
        request.buffer = k_invalid_buffer;
    }

    void AsynchronousLoader::request_buffer_upload(void* data, BufferHandle buffer) {

        UploadRequest& upload_request = upload_requests.push_use();
        upload_request.data = data;
        upload_request.gpu_buffer = buffer;
        upload_request.cpu_buffer = k_invalid_buffer;
        upload_request.texture = k_invalid_texture;
    }

    void AsynchronousLoader::request_buffer_copy(BufferHandle src, BufferHandle dst, u32* completed) {

        UploadRequest& upload_request = upload_requests.push_use();
        upload_request.completed = completed;
        upload_request.data = nullptr;
        upload_request.cpu_buffer = src;
        upload_request.gpu_buffer = dst;
        upload_request.texture = k_invalid_texture;
    }

} // namespace Helix
