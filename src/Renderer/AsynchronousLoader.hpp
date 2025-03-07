#pragma once

#include "Core/Array.hpp"
#include "Core/Platform.hpp"

#include "Renderer/CommandBuffer.hpp"
#include "Renderer/GPUDevice.hpp"
#include "Renderer/GPUResources.hpp"

#include <atomic>

namespace enki { class TaskScheduler; }

namespace Helix
{
    struct Allocator;
    struct FrameGraph;
    struct GPUProfiler;
    struct ImGuiService;
    struct Renderer;
    struct StackAllocator;

    //
    //
    struct FileLoadRequest {

        char                                    path[512];
        TextureHandle                           texture = k_invalid_texture;
        BufferHandle                            buffer = k_invalid_buffer;
    }; // struct FileLoadRequest

    //
    //
    struct UploadRequest {

        void*                                   data = nullptr;
        u32*                                    completed = nullptr;
        TextureHandle                           texture = k_invalid_texture;
        BufferHandle                            cpu_buffer = k_invalid_buffer;
        BufferHandle                            gpu_buffer = k_invalid_buffer;
    }; // struct UploadRequest

    //
    //
    struct AsynchronousLoader {

        void                                    init(Renderer* renderer, enki::TaskScheduler* task_scheduler, Allocator* resident_allocator);
        void                                    update(Allocator* stack_allocator);
        void                                    shutdown();

        void                                    request_texture_data(cstring filename, TextureHandle texture);
        void                                    request_buffer_upload(void* data, BufferHandle buffer);
        void                                    request_buffer_copy(BufferHandle src, BufferHandle dst, u32* completed);

        Allocator*                              allocator = nullptr;
        Renderer*                               renderer = nullptr;
        enki::TaskScheduler*                    task_scheduler = nullptr;

        Array<FileLoadRequest>                  file_load_requests;
        Array<UploadRequest>                    upload_requests;

        Buffer*                                 staging_buffer = nullptr;

        std::atomic_size_t                      staging_buffer_offset;
        TextureHandle                           texture_ready;
        BufferHandle                            cpu_buffer_ready;
        BufferHandle                            gpu_buffer_ready;
        u32*                                    completed;

        VkCommandPool                           command_pools[k_max_frames];
        CommandBuffer                           command_buffers[k_max_frames];
        VkSemaphore                             transfer_complete_semaphore;
        VkFence                                 transfer_fence;

    }; // struct AsynchonousLoader

} // namespace Helix
