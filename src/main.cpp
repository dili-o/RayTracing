
#include "Application/Window.hpp"
#include "Application/Input.hpp"
#include "Application/Keys.hpp"


#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <vendor/glm/glm/glm.hpp>
#include <vendor/glm/glm/gtc/matrix_transform.hpp>
#include <vendor/glm/glm/gtx/hash.hpp>



#include "vendor/imgui/imgui.h"

#include "vendor/tracy/tracy/Tracy.hpp"

#include "Renderer/GPUDevice.hpp"
#include "Renderer/CommandBuffer.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/HelixImgui.hpp"
#include "Renderer/GPUProfiler.hpp"
#include "Renderer/AsynchronousLoader.hpp"
#include "Renderer/Scene.hpp"
#include "Renderer/FrameGraph.hpp"
#include "Renderer/ResourcesLoader.hpp"


#include "Core/File.hpp"
#include "Core/Gltf.hpp"
#include "Core/Numerics.hpp"
#include "Core/ResourceManager.hpp"
#include "Core/Time.hpp"

#include <stdlib.h> // for exit()



f32 rx, ry;

enum MaterialFeatures {
    MaterialFeatures_ColorTexture = 1 << 0,
    MaterialFeatures_NormalTexture = 1 << 1,
    MaterialFeatures_RoughnessTexture = 1 << 2,
    MaterialFeatures_OcclusionTexture = 1 << 3,
    MaterialFeatures_EmissiveTexture = 1 << 4,

    MaterialFeatures_TangentVertexAttribute = 1 << 5,
    MaterialFeatures_TexcoordVertexAttribute = 1 << 6,
};

static void input_os_messages_callback(void* os_event, void* user_data) {
    Helix::InputService* input = (Helix::InputService*)user_data;
    input->on_event(os_event);
}

// [TAG: MULTITHREADING]
// IOTasks ////////////////////////////////////////////////////////////////
//
//
struct RunPinnedTaskLoopTask : enki::IPinnedTask {

    void Execute() override {
        while (task_scheduler->GetIsRunning() && execute) {
            task_scheduler->WaitForNewPinnedTasks(); // this thread will 'sleep' until there are new pinned tasks
            task_scheduler->RunPinnedTasks();
        }
    }

    enki::TaskScheduler*    task_scheduler;
    bool                    execute = true;
}; // struct RunPinnedTaskLoopTask

struct AsynchronousLoadTask : enki::IPinnedTask {

    void Execute() override {
        // Do file IO
        while (execute) {
            async_loader->update(nullptr);
        }
    }

    Helix::AsynchronousLoader*     async_loader;
    enki::TaskScheduler*    task_scheduler;
    bool                    execute = true;
}; // struct AsynchronousLoadTask

glm::vec4 normalize_plane(glm::vec4 plane) {
    glm::vec3 normal( plane.x, plane.y, plane.z);
    return (plane / glm::length(normal));
}



int main(int argc, char** argv)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	
	using namespace Helix;
	// Init services
	LogService log{};
	log.init();
    Time::service_init();

    MemoryServiceConfiguration memory_configuration;
    memory_configuration.maximum_dynamic_size = hmega(500);

	MemoryService::instance()->init(&memory_configuration);
	Allocator* allocator = &MemoryService::instance()->system_allocator;

	StackAllocator stack_allocator;
	stack_allocator.init(hmega(200));

    // [TAG: MULTITHREADING]
    enki::TaskSchedulerConfig config;
    config.numTaskThreadsToCreate = 4;
    enki::TaskScheduler task_scheduler;
    task_scheduler.Initialize(config);

	// window
	WindowConfiguration wconf{ 1280, 800, "Engine Test", allocator };
	Window window;
	window.init(&wconf);

	InputService input_handler;
	input_handler.init(allocator);

	// Callback register
	window.register_os_messages_callback(input_os_messages_callback, &input_handler);

	// graphics
	DeviceCreation dc;
	dc.set_window(window.width, window.height, window.platform_handle)
        .set_allocator(allocator)
        .set_num_threads(task_scheduler.GetNumTaskThreads())
        .set_linear_allocator(&stack_allocator);
	GpuDevice gpu;
	gpu.init(dc);

	ResourceManager rm;
	rm.init(allocator, nullptr);

	GPUProfiler gpu_profiler;
	gpu_profiler.init(allocator, 100);

	Renderer renderer;
	renderer.init({ &gpu, allocator });
	//renderer.set_loaders(&rm);

	ImGuiService* imgui = ImGuiService::instance();
	ImGuiServiceConfiguration imgui_config{ &gpu, window.platform_handle };
	imgui->init(&imgui_config);

    // FrameGraph
    FrameGraphBuilder frame_graph_builder;
    frame_graph_builder.init(&gpu);

    FrameGraph frame_graph;
    frame_graph.init(&frame_graph_builder);

    ResourcesLoader resources_loader;
    {
        {
            sizet scratch_marker = stack_allocator.get_marker();

            StringBuffer temporary_name_buffer;
            temporary_name_buffer.init(1024, &stack_allocator);
            cstring frame_graph_path = temporary_name_buffer.append_use_f(HELIX_FRAMEGRAPH_FOLDER"graph.json");

            frame_graph.parse(frame_graph_path, &stack_allocator);
            frame_graph.compile();

            resources_loader.init(&renderer, &stack_allocator, &frame_graph);

            // Parse programs
            temporary_name_buffer.clear();
            cstring full_screen_pipeline_path = temporary_name_buffer.append_use_f("%s/%s", HELIX_SHADER_FOLDER, "programs/fullscreen.json");
            resources_loader.load_program(full_screen_pipeline_path);

            temporary_name_buffer.clear();
            cstring pbr_pipeline_path = temporary_name_buffer.append_use_f("%s/%s", HELIX_SHADER_FOLDER, "programs/ray_tracing.json");
            resources_loader.load_program(pbr_pipeline_path);

            stack_allocator.free_marker(scratch_marker);
        }
    }
    

    // [TAG: Multithreading]
    AsynchronousLoader async_loader;
    async_loader.init(&renderer, &task_scheduler, allocator);

	Directory cwd{ };
	directory_current(&cwd);

    // TODO: Implement obj loading.
    glTFScene* scene = nullptr;
    scene = new glTFScene;

    scene->init(&renderer, allocator, &frame_graph, &stack_allocator, &async_loader);
    //scene->load(gltf_file, gltf_base_path, allocator, &stack_allocator, &async_loader);

    directory_change(cwd.path);

    scene->register_render_passes(&frame_graph);
    scene->prepare_draws(&renderer, &stack_allocator);

    directory_change(cwd.path);

    // [TAG: MULTITHREADING]
    // Create IO threads at the end
    RunPinnedTaskLoopTask run_pinned_task;
    run_pinned_task.threadNum = task_scheduler.GetNumTaskThreads() - 1;
    run_pinned_task.task_scheduler = &task_scheduler;
    task_scheduler.AddPinnedTask(&run_pinned_task);

    // Send async load task to external thread FILE_IO
    AsynchronousLoadTask async_load_task;
    async_load_task.threadNum = run_pinned_task.threadNum;
    async_load_task.task_scheduler = &task_scheduler;
    async_load_task.async_loader = &async_loader;
    task_scheduler.AddPinnedTask(&async_load_task);

    i64 begin_frame_tick = Time::now();
    i64 absolute_begin_frame_tick = begin_frame_tick;

    glm::vec3 eye = { 0.0f, 2.0f, -5.0f };
    glm::vec3 look = { 0.0f, 0.0, -1.0f };
    glm::vec3 right = { 1.0f, 0.0, 0.0f };
    glm::vec3 up = { 0.0f, 1.0f, 0.0f };

    f32 yaw = 0.0f;
    f32 pitch = 0.0f;

    float model_scale = 1.0f;
    float light_range = 300.0f;
    float light_intensity = 1000.f;

    int frame_count = 0;
    double last_time = 0.0;

    float aspect_ratio = 16.0f / 9.0f;// gpu.swapchain_width * 1.0f / gpu.swapchain_height;

    while (!window.requested_exit) {
        ZoneScoped;
        window.handle_os_messages();
        input_handler.new_frame();

        const i64 current_tick = Time::now();
        
        static bool freeze_occlusion_camera = false;
        static glm::mat4 projection_transpose;

        // New frame
        if (!window.minimized) {
            gpu.new_frame();
            static bool checksz = true;
            if (async_loader.file_load_requests.size == 0 && checksz) {
                checksz = false;
                HINFO("Finished uploading textures in {} seconds", Time::from_seconds(absolute_begin_frame_tick));
                //char* statsString = nullptr;
                //// Build the stats string with detailed information
                //vmaBuildStatsString(allocator, &statsString, VK_TRUE);
                //printf("====================================================================================");
                //// Print the statistics
                //printf("%s\n", statsString);
                //// Free the stats string when done
                //vmaFreeStatsString(allocator, statsString
            }
            if (window.resized) {
                gpu.resize(window.width, window.height);
                window.resized = false;
            }
            // This MUST be AFTER os messages!
            imgui->new_frame();

            f32 delta_time = (f32)Time::delta_seconds(begin_frame_tick, current_tick);
            begin_frame_tick = current_tick;

            input_handler.update(delta_time);

            {
                ZoneScopedN("ImGui Recording");

                ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
              /*  if (ImGui::Begin("Scene")) {
                    ImGui::InputFloat("Model scale", &model_scale, 0.001f);
                    ImGui::SliderFloat3("Light position", light_position.raw, -30.f, 30.f);
                    ImGui::InputFloat3("Camera position", eye.raw);
                    ImGui::InputFloat("Light range", &light_range);
                    ImGui::InputFloat("Light intensity", &light_intensity);
                }
                ImGui::End();*/

                if (ImGui::Begin("Viewport")) {
                    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
                    float aspect = 16.f / 9.f;
                    float v_apsect = viewportPanelSize.x / viewportPanelSize.y;
                    ImGui::Image((ImTextureID)&gpu.fullscreen_texture_handle, { viewportPanelSize.x, viewportPanelSize.x / v_apsect });
                    //aspect_ratio = v_apsect;// viewportPanelSize.x / viewportPanelSize.y;
                    aspect_ratio = viewportPanelSize.x / (viewportPanelSize.x / aspect);
                    //scene->ray_tracing_pass.viewport = { viewportPanelSize.x, viewportPanelSize.y };
                }
                ImGui::End();

                if (ImGui::Begin("Scene Settings")) {
                    if (ImGui::InputInt("Node Index", (i32*)&scene->ray_tracing_pass.node_index, 1)) {
                        scene->ray_tracing_pass.camera_moved = true;
                    }
                    if (ImGui::SliderInt("Node Index", (i32*)&scene->ray_tracing_pass.pad, 0, 1)) {
                        scene->ray_tracing_pass.camera_moved = true;
                    }


                    MapBufferParameters cb_map = { scene->scene_info_buffer, 0, 0 };
                    SceneInfo* info_data = (SceneInfo*)renderer.gpu->map_buffer(cb_map);
                    if (info_data) {
                        ImGui::Text("Total bounce: %d", info_data->total_bounce_count);
                        renderer.gpu->unmap_buffer(cb_map);
                    }

                    ImGui::Text("x: %f, y: %f, z: %f", eye.x, eye.y, eye.z);
                    ImGui::Text("AABB \t minx: %f, miny: %f, maxx: %f, maxy: %f", scene->tester.x, scene->tester.y, scene->tester.z, scene->tester.w);
                    ImGui::Checkbox("Freeze Camera", &freeze_occlusion_camera);
                }
                ImGui::End();

                //MemoryService::instance()->imgui_draw();

                if (ImGui::Begin("GPU")) {
                    renderer.imgui_draw();

                    ImGui::Separator();
                    gpu_profiler.imgui_draw();
                }
                ImGui::End();

                scene->imgui_draw_hierarchy();
                
                renderer.imgui_resources_draw();

                scene->imgui_draw_node_property(scene->current_node);

            }

            {
                
                MapBufferParameters light_cb_map = { scene->light_cb, 0, 0 };
                //LightUniform* light_cb_data = (LightUniform*)gpu.map_buffer(light_cb_map);
                {
                    if (input_handler.is_mouse_down(MouseButtons::MOUSE_BUTTONS_RIGHT)) {
                        float mouse_sensitivity = 0.15f;

                        pitch += (input_handler.mouse_position.y - input_handler.previous_mouse_position.y) * mouse_sensitivity / 2.f;
                        yaw += (input_handler.mouse_position.x - input_handler.previous_mouse_position.x) * mouse_sensitivity;

                        pitch = clamp(pitch, -60.0f, 60.0f);

                        if (yaw > 360.0f) {
                            yaw -= 360.0f;
                        }

                        glm::mat3 rxm = glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(-pitch), { 1.0f, 0.0f, 0.0f }));
                        glm::mat3 rym = glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(-yaw), { 0.0f, 1.0f, 0.0f }));

                        look = rxm * glm::vec3( 0.0f, 0.0f, -1.0f );
                        look = rym * look;

                        right = glm::cross(look, glm::vec3( 0.0f, 1.0f, 0.0f ));

                        scene->ray_tracing_pass.camera_moved = true;
                    }

                    float move_speed = 13.f;
                    if (input_handler.is_key_down(Keys::KEY_W)) {
                        eye += look * (move_speed * delta_time);
                        scene->ray_tracing_pass.camera_moved = true;
                    }
                    else if (input_handler.is_key_down(Keys::KEY_S)) {
                        eye -= look * (move_speed * delta_time);
                        scene->ray_tracing_pass.camera_moved = true;
                    }

                    if (input_handler.is_key_down(Keys::KEY_D)) {
                        eye += right * (move_speed * delta_time);
                        scene->ray_tracing_pass.camera_moved = true;
                    }
                    else if (input_handler.is_key_down(Keys::KEY_A)) {
                        eye -= right * (move_speed * delta_time);
                        scene->ray_tracing_pass.camera_moved = true;
                    }
                    if (input_handler.is_key_down(Keys::KEY_E)) {
                        eye += up * (move_speed * delta_time);
                        scene->ray_tracing_pass.camera_moved = true;
                    }
                    else if (input_handler.is_key_down(Keys::KEY_Q)) {
                        eye -= up * (move_speed * delta_time);
                        scene->ray_tracing_pass.camera_moved = true;
                    }

                    //else if (input_handler.is_key_down(Keys::KEY_RIGHT)) {
                    //    scene->ray_tracing_pass.node_index = (scene->ray_tracing_pass.node_index + 1) % 9;
                    //    scene->ray_tracing_pass.camera_moved = true;
                    //}
                    //else if (input_handler.is_key_down(Keys::KEY_LEFT)) {
                    //    scene->ray_tracing_pass.node_index = (scene->ray_tracing_pass.node_index - 1 + 9) % 9;
                    //    scene->ray_tracing_pass.camera_moved = true;
                    //}

                    glm::mat4 view = glm::lookAt(eye, (eye + look), glm::vec3( 0.0f, 1.0f, 0.0f ));
                    float z_near = 0.001f;
                    float z_far = 1000.0f;
                    glm::mat4 projection = glm::perspective(glm::radians(45.f), aspect_ratio, z_near, z_far);

                    // Calculate view projection matrix
                    glm::mat4 view_projection = projection * view;

                    // Rotate cube:
                    rx += 1.0f * delta_time;
                    ry += 2.0f * delta_time;

                    glm::mat4 rxm = glm::rotate(glm::mat4(1.0f), rx, glm::vec3(1.0f, 0.0f, 0.0f));
                    glm::mat4 rym = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3( 0.0f, 1.0f, 0.0f ));

                    glm::mat4 sm = glm::scale(glm::mat4(1.0f), glm::vec3( model_scale));


                    GPUSceneData& scene_data = scene->scene_data;
                    scene_data.inverse_view = glm::inverse(view);
                    scene_data.inverse_projection = glm::inverse(projection);
                    scene_data.view_matrix = view;
                    scene_data.projection_matrix = projection;
                    scene_data.inverse_view_projection = projection;
                    scene_data.camera_position = glm::vec4(eye.x, eye.y, eye.z, 1.0f);
                    scene_data.light_position = glm::vec4(0.0f, 10.0f, 0.0f, 1.0f);
                    scene_data.dither_texture_index = k_invalid_index;

                    scene_data.z_near = z_near;
                    scene_data.z_far = z_far;
                    scene_data.projection_00 = projection[0][0];
                    scene_data.projection_11 = projection[1][1];

                    scene_data.frustum_cull_meshes = 1;
                    scene_data.frustum_cull_meshlets = 1;
                    scene_data.occlusion_cull_meshes = 1;
                    scene_data.occlusion_cull_meshlets = 1;
                    scene_data.freeze_occlusion_camera = freeze_occlusion_camera ? 1 : 0;

                    scene_data.seed_x = (f32)random_float();

                    scene_data.seed_y = (f32)random_float();


                    scene_data.aspect_ratio = gpu.swapchain_width * 1.f / gpu.swapchain_height;

                    // Frustum computations
                    if (!freeze_occlusion_camera) {
                        scene_data.camera_position_debug = scene_data.camera_position;
                        projection_transpose = glm::transpose(projection);
                    }

                    scene_data.frustum_planes[0] = normalize_plane(projection_transpose[3] + projection_transpose[0]); // x + w  < 0;
                    scene_data.frustum_planes[1] = normalize_plane(projection_transpose[3] - projection_transpose[0]); // x - w  < 0;
                    scene_data.frustum_planes[2] = normalize_plane(projection_transpose[3] + projection_transpose[1]); // y + w  < 0;
                    scene_data.frustum_planes[3] = normalize_plane(projection_transpose[3] - projection_transpose[1]); // y - w  < 0;
                    scene_data.frustum_planes[4] = normalize_plane(projection_transpose[3] + projection_transpose[2]); // z + w  < 0;
                    scene_data.frustum_planes[5] = normalize_plane(projection_transpose[3] - projection_transpose[2]); // z - w  < 0;


                    MapBufferParameters cb_map = { scene->scene_constant_buffer, 0, 0 };
                    GPUSceneData* cb_data = (GPUSceneData*)gpu.map_buffer(cb_map);
                    if (cb_data) {
                        memcpy(cb_data, &scene->scene_data, sizeof(GPUSceneData));

                        gpu.unmap_buffer(cb_map);
                    }
                    

                    glm::mat4 model = glm::mat4(1.0f);

                    gpu.unmap_buffer(light_cb_map);
                }
                
                scene->fill_gpu_data_buffers(model_scale);
            }
            scene->submit_draw_task(imgui, &gpu_profiler, &task_scheduler);

            gpu.present(nullptr);

            f64 current_time = Time::from_seconds(absolute_begin_frame_tick);
            frame_count++;
            if (current_time - last_time >= 1.0) {
                renderer.fps = (f64)frame_count / (current_time - last_time);
            
                // Reset for the next second
                last_time = current_time;
                frame_count = 0;
            }
        }
    }

    run_pinned_task.execute = false;
    async_load_task.execute = false;

    task_scheduler.WaitforAllAndShutdown();

    vkDeviceWaitIdle(gpu.vulkan_device);

    async_loader.shutdown();

    //gpu.destroy_buffer(scene->scene_constant_buffer);
    //gpu.destroy_buffer(scene->light_cb);

    imgui->shutdown();

    gpu_profiler.shutdown();

    frame_graph.shutdown();
    frame_graph_builder.shutdown();

    scene->free_gpu_resources(&renderer);

    resources_loader.shutdown();

    rm.shutdown();
    renderer.shutdown();

    scene->unload(&renderer);
    delete scene;

    input_handler.shutdown();
    window.unregister_os_messages_callback(input_os_messages_callback);
    window.shutdown();

    stack_allocator.shutdown();
    MemoryService::instance()->shutdown();

    return 0;
}

