#include "Scene.hpp"

#include <vendor/imgui/stb_image.h>

#include "Core/Gltf.hpp"
#include "Core/Time.hpp"
#include "Core/File.hpp"

#include <vendor/tracy/tracy/Tracy.hpp>
#include <vendor/meshoptimizer/meshoptimizer.h>

#include <imgui/imgui.h>
#include <Core/Numerics.hpp>

namespace Helix {
    float square(float r) {
        return r * r;
    }

    glm::vec4 project_sphere(glm::vec3 C, float r, float znear, float P00, float P11, glm::mat4 P) {
        
        if (C.z + r > -znear)
            return glm::vec4(0, 0, 0, 0);

        glm::vec4 aabb;

        glm::vec4 min_p = glm::vec4(C.x - r, C.y - r, C.z, 1.0f);
        glm::vec4 max_p = glm::vec4(C.x + r, C.y + r, C.z, 1.0f);

        glm::vec4 min_clip = P * min_p;
        glm::vec4 max_clip = P * max_p;

        //aabb = glm::vec4(min_clip.x / min_clip.w, min_clip.y / min_clip.w, max_clip.x / max_clip.w, max_clip.y / max_clip.w);

        //aabb = glm::vec4(aabb.x + 1.0f, 1.0f - aabb.y, aabb.z + 1.0f, 1.0f - aabb.w) * 0.5f;

        glm::vec2 cx = glm::vec2(C.x, -C.z);
        glm::vec2 vx = glm::vec2(sqrt(dot(cx, cx) - r * r), r);
        glm::vec2 minx = glm::mat2(vx.x, vx.y, -vx.y, vx.x) * cx;
        glm::vec2 maxx = glm::mat2(vx.x, -vx.y, vx.y, vx.x) * cx;

        glm::vec2 cy = glm::vec2(-C.y, -C.z);
        glm::vec2 vy = glm::vec2(sqrt(dot(cy, cy) - r * r), r);
        glm::vec2 miny = glm::mat2(vy.x, vy.y, -vy.y, vy.x) * cy;
        glm::vec2 maxy = glm::mat2(vy.x, -vy.y, vy.y, vy.x) * cy;

        aabb = glm::vec4(minx.x / minx.y * P00, miny.x / miny.y * P11, maxx.x / maxx.y * P00, maxy.x / maxy.y * P11);
        aabb = aabb * glm::vec4(0.5f, -0.5f, 0.5f, -0.5f) + glm::vec4(0.5f); // clip space -> uv space

        return aabb;
    }


    // Node
    static void update_transform(Node* node, NodePool* node_pool) {

        if (node->parent.index != k_invalid_index) {
            Node* parent_node = (Node*)node_pool->access_node(node->parent);
            glm::mat4 combined_matrix = parent_node->world_transform.calculate_matrix() * node->local_transform.calculate_matrix();
            node->world_transform.set_transform(combined_matrix);
        }
        else {
            node->world_transform.set_transform(node->local_transform.calculate_matrix());
        }

        for (u32 i = 0; i < node->children.size; i++) {
            Node* child_node = (Node*)node_pool->access_node(node->children[i]);
            child_node->update_transform(node_pool);
        }
    }

    static void update_mesh_transform(Node* node, NodePool* node_pool) {
        update_transform(node, node_pool);

        MeshNode* mesh_node = static_cast<MeshNode*>(node);
        //mesh_node->gpu_mesh_data->model = node->world_transform.calculate_matrix();
    }

    // Helper functions //////////////////////////////////////////////////

    // Light
    //Helix::PipelineHandle                  light_pipeline;


    void get_mesh_vertex_buffer(glTFScene& scene, i32 accessor_index, BufferHandle& out_buffer_handle, u32& out_buffer_offset) {

        if (accessor_index != -1) {
            glTF::Accessor& buffer_accessor = scene.gltf_scene.accessors[accessor_index];
            glTF::BufferView& buffer_view = scene.gltf_scene.buffer_views[buffer_accessor.buffer_view];
            BufferResource& buffer_gpu = scene.buffers[buffer_accessor.buffer_view + scene.current_buffers_count];


            out_buffer_handle = buffer_gpu.handle;
            out_buffer_offset = buffer_accessor.byte_offset == glTF::INVALID_INT_VALUE ? 0 : buffer_accessor.byte_offset;
        }
        else {
            // TODO: Right now if a glTF model doesn't have a vertex buffer we just bind the 0 index buffer
            out_buffer_handle.index = 0;
        }
    }

    int gltf_mesh_material_compare(const void* a, const void* b) {
        const Mesh* mesh_a = (const Mesh*)a;
        const Mesh* mesh_b = (const Mesh*)b;

        if (mesh_a->pbr_material.material->render_index < mesh_b->pbr_material.material->render_index) return -1;
        if (mesh_a->pbr_material.material->render_index > mesh_b->pbr_material.material->render_index) return  1;
        return 0;
    }

    int gltf_mesh_doublesided_compare(const void* a, const void* b) {
        const Mesh* mesh_a = static_cast<const Mesh*>(a);
        const Mesh* mesh_b = static_cast<const Mesh*>(b);

        // Sort double-sided meshes first.
        if (mesh_a->is_double_sided() && !mesh_b->is_double_sided()) {
            return -1;  // meshA comes before meshB
        }
        if (!mesh_a->is_double_sided() && mesh_b->is_double_sided()) {
            return 1;   // meshB comes before meshA
        }
        return 0; // both are the same (both double-sided or both not double-sided)
    }

    static void copy_gpu_material_data(GPUMaterialData& gpu_mesh_data, const Mesh& mesh) {
        gpu_mesh_data.textures[0] = mesh.pbr_material.diffuse_texture_index;
        gpu_mesh_data.textures[1] = mesh.pbr_material.roughness_texture_index;
        gpu_mesh_data.textures[2] = mesh.pbr_material.normal_texture_index;
        gpu_mesh_data.textures[3] = mesh.pbr_material.occlusion_texture_index;

        //gpu_mesh_data.emissive = { mesh.pbr_material.emissive_factor.x, mesh.pbr_material.emissive_factor.y, mesh.pbr_material.emissive_factor.z, (float)mesh.pbr_material.emissive_texture_index };

        gpu_mesh_data.base_color_factor = mesh.pbr_material.base_color_factor;
        gpu_mesh_data.metallic_roughness_occlusion_factor = mesh.pbr_material.metallic_roughness_occlusion_factor;
        gpu_mesh_data.alpha_cutoff = mesh.pbr_material.alpha_cutoff;

        //gpu_mesh_data.diffuse_colour = mesh.pbr_material.diffuse_colour;
        //gpu_mesh_data.specular_colour = mesh.pbr_material.specular_colour;
        //gpu_mesh_data.specular_exp = mesh.pbr_material.specular_exp;
        //gpu_mesh_data.ambient_colour = mesh.pbr_material.ambient_colour;

        gpu_mesh_data.flags = mesh.pbr_material.flags;

        gpu_mesh_data.mesh_index = mesh.gpu_mesh_index;
        gpu_mesh_data.meshlet_offset = mesh.meshlet_offset;
        gpu_mesh_data.meshlet_count = mesh.meshlet_count;
    }
    //
    //
    static FrameGraphResource* get_output_texture(FrameGraph* frame_graph, FrameGraphResourceHandle input) {
        FrameGraphResource* input_resource = frame_graph->access_resource(input);

        FrameGraphResource* output_resource = frame_graph->access_resource(input_resource->output_handle);
        HASSERT(output_resource != nullptr);

        return output_resource;
    }

    //
    //
    static void copy_gpu_mesh_matrix(GPUMeshData& gpu_mesh_data, const Mesh& mesh, const f32 global_scale, const ResourcePool* mesh_nodes) {

        // Apply global scale matrix
        glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), glm::vec3(global_scale, global_scale, global_scale));
        MeshNode* mesh_node = (MeshNode*)mesh_nodes->access_resource(mesh.node_index);
        gpu_mesh_data.model = mesh_node->world_transform.calculate_matrix() * scale_mat;
        gpu_mesh_data.inverse_model = glm::inverse(glm::transpose(gpu_mesh_data.model));
    }


    //
    // RayTracingPass /////////////////////////////////////////////////////////
    void RayTracingPass::render(CommandBuffer* gpu_commands, Scene* scene_) {

        if (!enabled)
            return;

        glTFScene* scene = (glTFScene*)scene_;
        Renderer* renderer = scene->renderer;

        gpu_commands->bind_pipeline(pipeline_handle);

        gpu_commands->bind_descriptor_set(&d_set, 1, nullptr, 0);

        const Pipeline* rtx_pipeline = renderer->gpu->access_pipeline(pipeline_handle);
        const Texture* texture = renderer->gpu->access_texture({ output_image_index });

        struct PushConst {
            glm::vec2 src_image_size;
        };
        PushConst push_const;
        push_const.src_image_size = viewport;

        vkCmdPushConstants(gpu_commands->vk_handle, rtx_pipeline->vk_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConst), &push_const);

        u32 group_x = (texture->width + rtx_pipeline->local_size[0] - 1) / rtx_pipeline->local_size[0];
        u32 group_y = (texture->height + rtx_pipeline->local_size[1] - 1) / rtx_pipeline->local_size[1];
        gpu_commands->dispatch(group_x, group_y, 1);
    }

    void RayTracingPass::prepare_draws(Scene& scene, FrameGraph* frame_graph, Allocator* resident_allocator) {

        FrameGraphNode* node = frame_graph->get_node("ray_tracing_pass");
        if (node == nullptr) {
            enabled = false;

            return;
        }
        enabled = node->enabled;
                
        renderer = scene.renderer;
        GpuDevice& gpu = *renderer->gpu;

        output_image_index = scene.fullscreen_texture_index;

        // Cache frustum cull shader
        Program* rtx_program = renderer->resource_cache.programs.get(hash_calculate("ray_tracing"));
        {

            u32 pipeline_index = rtx_program->get_pass_index("ray_tracing");
            pipeline_handle = rtx_program->passes[pipeline_index].pipeline;
            DescriptorSetLayoutHandle layout = gpu.get_descriptor_set_layout(pipeline_handle, k_material_descriptor_set_index);
            DescriptorSetCreation ds_creation{};
            ds_creation
                .buffer(scene.scene_constant_buffer, 0)
                .texture({ output_image_index }, 1) 
                .set_layout(layout);

            d_set = gpu.create_descriptor_set(ds_creation);
        }
    }

    void RayTracingPass::free_gpu_resources() {
        if (!renderer)
            return;
        GpuDevice& gpu = *renderer->gpu;
        gpu.destroy_descriptor_set(d_set);
    }


    cstring node_type_to_cstring(NodeType type) {
        switch (type)
        {
        case Helix::NodeType::Node:
            return "Node";
            break;
        case Helix::NodeType::MeshNode:
            return "Mesh Node";
            break;
        case Helix::NodeType::LightNode:
            return "Light Node";
            break;
        default:
            HCRITICAL("Invalid node type");
            return "Invalid node type";
            break;
        }
    }


    // glTFDrawTask //////////////////////////////////

    void glTFDrawTask::init(GpuDevice* gpu_, FrameGraph* frame_graph_, Renderer* renderer_, ImGuiService* imgui_, GPUProfiler* gpu_profiler_, glTFScene* scene_) {
        gpu = gpu_;
        frame_graph = frame_graph_;
        renderer = renderer_;
        imgui = imgui_;
        gpu_profiler = gpu_profiler_;
        scene = scene_;
    }

    void glTFDrawTask::ExecuteRange(enki::TaskSetPartition range_, uint32_t threadnum_) {
        ZoneScoped;

        thread_id = threadnum_;

        //HTRACE( "Executing draw task from thread {}", threadnum_ );
        // TODO: improve getting a command buffer/pool
        CommandBuffer* gpu_commands = gpu->get_command_buffer(threadnum_, true);

        frame_graph->render(gpu->current_frame, gpu_commands, scene);


        gpu_commands->push_marker("Fullscreen");
        gpu_commands->clear(0.3f, 0.3f, 0.3f, 1.f);
        gpu_commands->clear_depth_stencil(1.0f, 0);
        
        Framebuffer* fullscreen_fb = gpu->access_framebuffer(gpu->fullscreen_framebuffer);
        Texture* fullscreen_texture = gpu->access_texture(fullscreen_fb->color_attachments[0]);

        //util_add_image_barrier(gpu, gpu_commands->vk_handle, fullscreen_texture, RESOURCE_STATE_RENDER_TARGET, 0, 1, false);

        gpu_commands->bind_pass(gpu->fullscreen_render_pass, gpu->fullscreen_framebuffer, false);
        gpu_commands->set_scissor(nullptr);
        Viewport viewport{};
        viewport.rect = { 0,0,gpu->swapchain_width, gpu->swapchain_height };
        viewport.max_depth = 1.0f;
        viewport.min_depth = 0.0f;

        gpu_commands->set_viewport(&viewport);
        gpu_commands->bind_pipeline(scene->fullscreen_program->passes[0].pipeline);
        gpu_commands->bind_descriptor_set(&scene->fullscreen_ds, 1, nullptr, 0);
        gpu_commands->draw(TopologyType::Triangle, 0, 3, scene->fullscreen_texture_index, 1);

        gpu_commands->end_current_render_pass();

        gpu_commands->pop_marker();

        imgui->render(*gpu_commands, false);
        gpu_commands->end_current_render_pass();

        gpu_commands->pop_marker();

        gpu_profiler->update(*gpu);

        // Send commands to GPU
        gpu->queue_command_buffer(gpu_commands);
    }

    // gltfScene //////////////////////////////////////////////////

    void glTFScene::init(Renderer* _renderer, Allocator* resident_allocator, FrameGraph* _frame_graph, StackAllocator* stack_allocator, AsynchronousLoader* async_loader) {
        u32 k_num_meshes = 200;
        renderer = _renderer;
        frame_graph = _frame_graph;
        scratch_allocator = stack_allocator;
        main_allocator = resident_allocator;
        loader = async_loader;

        transparent_meshes.init(resident_allocator, k_num_meshes);
        opaque_meshes.init(resident_allocator, k_num_meshes);

        node_pool.init(resident_allocator);

        images.init(resident_allocator, k_num_meshes);
        samplers.init(resident_allocator, 1);
        buffers.init(resident_allocator, k_num_meshes);

        meshlets.init(resident_allocator, 16);
        meshlet_vertex_and_index_indices.init(resident_allocator, 16);
        meshlets_vertex_positions.init(resident_allocator, 16);
        meshlets_vertex_data.init(resident_allocator, 16);

        names.init(hmega(1), main_allocator);

        // Creating the light image
        stbi_set_flip_vertically_on_load(true);
        TextureResource* tr = renderer->create_texture("Light", HELIX_TEXTURE_FOLDER"lights/point_light.png", true);
        stbi_set_flip_vertically_on_load(false);
        HASSERT(tr != nullptr);
        light_texture = *tr;

        // Constant buffer
        BufferCreation buffer_creation;
        buffer_creation.reset().set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceUsageType::Dynamic, sizeof(GPUSceneData)).set_name("scene_constant_buffer");
        scene_constant_buffer = renderer->create_buffer(buffer_creation)->handle;

        fullscreen_program = renderer->resource_cache.programs.get(hash_calculate("fullscreen"));

        DescriptorSetCreation dsc;
        DescriptorSetLayoutHandle descriptor_set_layout = renderer->gpu->get_descriptor_set_layout(fullscreen_program->passes[0].pipeline, k_material_descriptor_set_index);
        dsc.reset().buffer(scene_constant_buffer, 0).set_layout(descriptor_set_layout);
        fullscreen_ds = renderer->gpu->create_descriptor_set(dsc);

        FrameGraphResource* texture = frame_graph->get_resource("final");
        if (texture != nullptr) {
            fullscreen_texture_index = texture->resource_info.texture.handle.index;
        }
    }

    void glTFScene::free_gpu_resources(Renderer* renderer) {
        GpuDevice& gpu = *renderer->gpu;

        ray_tracing_pass.free_gpu_resources();

        gpu.destroy_descriptor_set(fullscreen_ds);

        
        transparent_meshes.shutdown();
        opaque_meshes.shutdown();
    }

    void glTFScene::unload(Renderer* renderer) {
        GpuDevice& gpu = *renderer->gpu;

        destroy_node(node_pool.root_node);

        node_pool.shutdown();
        // Free scene buffers
        samplers.shutdown();
        images.shutdown();
        buffers.shutdown();

        meshlets.shutdown();
        meshlets_vertex_positions.shutdown();
        meshlets_vertex_data.shutdown();
        meshlet_vertex_and_index_indices.shutdown();

        

        // NOTE(marco): we can't destroy this sooner as textures and buffers
        // hold a pointer to the names stored here
        gltf_free(gltf_scene);

        names.shutdown();
    }

    void glTFScene::register_render_passes(FrameGraph* frame_graph_) {
        frame_graph = frame_graph_;
        frame_graph->builder->register_render_pass("ray_tracing_pass", &ray_tracing_pass);
    }

    void glTFScene::prepare_draws(Renderer* renderer, StackAllocator* stack_allocator) {
        ray_tracing_pass.prepare_draws(*this, frame_graph, renderer->gpu->allocator);
    }

    void glTFScene::fill_pbr_material(Renderer& renderer, glTF::Material& material, PBRMaterial& pbr_material) {
        GpuDevice& gpu = *renderer.gpu;

        // Handle flags
        if (material.alpha_mode.data != nullptr && strcmp(material.alpha_mode.data, "MASK") == 0) {
            pbr_material.flags |= DrawFlags_AlphaMask;
        }
        else if (material.alpha_mode.data != nullptr && strcmp(material.alpha_mode.data, "BLEND") == 0) {
            pbr_material.flags |= DrawFlags_Transparent;
        }

        pbr_material.flags |= material.double_sided ? DrawFlags_DoubleSided : 0;
        // Alpha cutoff
        pbr_material.alpha_cutoff = material.alpha_cutoff != glTF::INVALID_FLOAT_VALUE ? material.alpha_cutoff : 1.f;

        if (material.pbr_metallic_roughness != nullptr) {
            if (material.pbr_metallic_roughness->base_color_factor_count != 0) {
                HASSERT(material.pbr_metallic_roughness->base_color_factor_count == 4);

                memcpy(&pbr_material.base_color_factor.x, material.pbr_metallic_roughness->base_color_factor, sizeof(glm::vec4));
            }
            else {
                pbr_material.base_color_factor = { 1.0f, 1.0f, 1.0f, 1.0f };
            }

            pbr_material.metallic_roughness_occlusion_factor.x = material.pbr_metallic_roughness->roughness_factor != glTF::INVALID_FLOAT_VALUE ? material.pbr_metallic_roughness->roughness_factor : 1.f;
            pbr_material.metallic_roughness_occlusion_factor.y = material.pbr_metallic_roughness->metallic_factor != glTF::INVALID_FLOAT_VALUE ? material.pbr_metallic_roughness->metallic_factor : 1.f;

            pbr_material.diffuse_texture_index = get_material_texture(gpu, material.pbr_metallic_roughness->base_color_texture);
            pbr_material.roughness_texture_index = get_material_texture(gpu, material.pbr_metallic_roughness->metallic_roughness_texture);
        }

        pbr_material.occlusion_texture_index = get_material_texture(gpu, (material.occlusion_texture != nullptr) ? material.occlusion_texture->index : -1);
        pbr_material.normal_texture_index = get_material_texture(gpu, (material.normal_texture != nullptr) ? material.normal_texture->index : -1);

        if (material.occlusion_texture != nullptr) {
            if (material.occlusion_texture->strength != glTF::INVALID_FLOAT_VALUE) {
                pbr_material.metallic_roughness_occlusion_factor.z = material.occlusion_texture->strength;
            }
            else {
                pbr_material.metallic_roughness_occlusion_factor.z = 1.0f;
            }
        }
    }

    u16 glTFScene::get_material_texture(GpuDevice& gpu, glTF::TextureInfo* texture_info) {
        if (texture_info != nullptr) {
            glTF::Texture& gltf_texture = gltf_scene.textures[texture_info->index];
            TextureResource& texture_gpu = images[gltf_texture.source + current_images_count];
            SamplerHandle sampler_gpu{};
            sampler_gpu = gpu.default_sampler;
            if (gltf_texture.sampler != 2147483647) {
                sampler_gpu = samplers[gltf_texture.sampler + current_samplers_count].handle;
            }

            gpu.link_texture_sampler(texture_gpu.handle, sampler_gpu);

            return (u16)texture_gpu.handle.index;
        }
        else {
            return (u16)k_invalid_index;
        }
    }

    u16 glTFScene::get_material_texture(GpuDevice& gpu, i32 gltf_texture_index) {
        if (gltf_texture_index >= 0) {
            glTF::Texture& gltf_texture = gltf_scene.textures[gltf_texture_index];
            TextureResource& texture_gpu = images[gltf_texture.source + current_images_count];
            SamplerHandle sampler_gpu{};
            sampler_gpu = gpu.default_sampler;
            if (gltf_texture.sampler != 2147483647) {
                sampler_gpu = samplers[gltf_texture.sampler + current_samplers_count].handle;
            }

            gpu.link_texture_sampler(texture_gpu.handle, sampler_gpu);

            return (u16)texture_gpu.handle.index;
        }
        else {
            return (u16)k_invalid_index;
        }
    }

    void glTFScene::fill_gpu_data_buffers(float model_scale) {
        // Update per mesh material buffer

        MapBufferParameters cb_map = { material_data_buffer, 0, 0 };
        GPUMaterialData* gpu_mesh_data = (GPUMaterialData*)renderer->gpu->map_buffer(cb_map);
        MapBufferParameters mesh_instance_map = { mesh_instances_buffer, 0, 0 };
        GPUMeshInstanceData* gpu_mesh_instance_data = (GPUMeshInstanceData*)renderer->gpu->map_buffer(mesh_instance_map);
        if (gpu_mesh_data) {
            for (u32 mesh_index = 0; mesh_index < opaque_meshes.size; ++mesh_index) {
                copy_gpu_material_data(gpu_mesh_data[mesh_index], opaque_meshes[mesh_index]);
                Mesh& mesh = opaque_meshes[mesh_index];

                MapBufferParameters material_buffer_map = { mesh.pbr_material.material_buffer, 0, 0 };
                GPUMeshData* mesh_data = (GPUMeshData*)renderer->gpu->map_buffer(material_buffer_map);
                if (mesh_data) {
                    copy_gpu_mesh_matrix(*mesh_data, mesh, model_scale, &node_pool.mesh_nodes);
                    gpu_mesh_instance_data[mesh_index].world = mesh_data->model;
                    gpu_mesh_instance_data[mesh_index].inverse_world = mesh_data->inverse_model;
                    gpu_mesh_instance_data[mesh_index].mesh_index = mesh_index;
                    renderer->gpu->unmap_buffer(material_buffer_map);
                }
            }
            
            renderer->gpu->unmap_buffer(cb_map);
            renderer->gpu->unmap_buffer(mesh_instance_map);
        }

        // Copy mesh bounding spheres
        cb_map.buffer = mesh_bounds_buffer;
        glm::vec4* gpu_bounds_data = (glm::vec4*)renderer->gpu->map_buffer(cb_map);
        if (gpu_bounds_data) {
            for (u32 mesh_index = 0; mesh_index < opaque_meshes.size; ++mesh_index) {
                gpu_bounds_data[mesh_index] = opaque_meshes[mesh_index].bounding_sphere;
            }
            renderer->gpu->unmap_buffer(cb_map);
        }
    }

    void glTFScene::submit_draw_task(ImGuiService* imgui, GPUProfiler* gpu_profiler, enki::TaskScheduler* task_scheduler) {
        glTFDrawTask draw_task;
        draw_task.init(renderer->gpu, frame_graph, renderer, imgui, gpu_profiler, this);
        task_scheduler->AddTaskSetToPipe(&draw_task);
        task_scheduler->WaitforTask(&draw_task);
        // Avoid using the same command buffer
        renderer->add_texture_update_commands((draw_task.thread_id + 1) % task_scheduler->GetNumTaskThreads());
    }

    void glTFScene::draw_mesh(CommandBuffer* gpu_commands, Mesh& mesh) {

        gpu_commands->bind_vertex_buffer(mesh.position_buffer, 0, mesh.position_offset);
        gpu_commands->bind_vertex_buffer(mesh.tangent_buffer, 1, mesh.tangent_offset);
        gpu_commands->bind_vertex_buffer(mesh.normal_buffer, 2, mesh.normal_offset);
        gpu_commands->bind_vertex_buffer(mesh.texcoord_buffer, 3, mesh.texcoord_offset);
        gpu_commands->bind_index_buffer(mesh.index_buffer, mesh.index_offset, mesh.index_type);

        gpu_commands->bind_descriptor_set(&mesh.pbr_material.descriptor_set, 1, nullptr, 0);

        gpu_commands->draw_indexed(TopologyType::Triangle, mesh.primitive_count, 1, 0, 0, 0);
    }

    void glTFScene::destroy_node(NodeHandle handle) {
        Node* node = (Node*)node_pool.access_node(handle);
        for (u32 i = 0; i < node->children.size; i++) {
            destroy_node(node->children[i]);
        }
        node->children.shutdown();
        switch (handle.type)
        {
        case NodeType::Node:
            node_pool.base_nodes.release_resource(handle.index);
            break;
        case NodeType::MeshNode:
            node_pool.mesh_nodes.release_resource(handle.index);
            break;
        case NodeType::LightNode:
            node_pool.light_nodes.release_resource(handle.index);
            break;
        default:
            HERROR("Invalid NodeType");
            break;
        }
    }

    void glTFScene::imgui_draw_node_property(NodeHandle node_handle) {
        if (ImGui::Begin("Node Properties")) {
            if (node_handle.index == k_invalid_index) {
                ImGui::Text("No node selected");
            }
            else {
                Node* node = (Node*)node_pool.access_node(node_handle);
                ImGui::Text("Name: %s", node->name);
                ImGui::Text("Type: %s", node_type_to_cstring(node_handle.type));

                if (!(node->parent.index == k_invalid_index)) {
                    Node* parent = (Node*)node_pool.access_node(node->parent);
                    ImGui::Text("Parent: %s", parent->name);
                }

                bool modified = false;

                glm::vec3 local_rotation = glm::degrees(glm::eulerAngles(node->local_transform.rotation));
                glm::vec3 world_rotation = glm::degrees(glm::eulerAngles(node->world_transform.rotation));

                // TODO: Represent rotation as quats
                ImGui::Text("Local Transform");
                modified |= ImGui::InputFloat3("position##local", (float*)&node->local_transform.translation);
                modified |= ImGui::InputFloat3("scale##local", (float*)&node->local_transform.scale);
                modified |= ImGui::InputFloat3("rotation##local", (float*)&local_rotation);

                ImGui::Text("World Transform");
                ImGui::InputFloat3("position##world", (float*)&node->world_transform.translation);
                ImGui::InputFloat3("scale##world", (float*)&node->world_transform.scale);
                ImGui::InputFloat3("rotation##world", (float*)&world_rotation);

                if (modified) {
                    node->local_transform.rotation = glm::quat(glm::radians(local_rotation));
                    node->update_transform(&node_pool);
                }
            }
        }
        ImGui::End();
    }

    void glTFScene::imgui_draw_node(NodeHandle node_handle) {
        Node* node = (Node*)node_pool.access_node(node_handle);

        if (node->name == nullptr)
            return;
        // Make a tree node for nodes with children
        ImGuiTreeNodeFlags tree_node_flags = 0;
        tree_node_flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (!node->children.size) {
            tree_node_flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (ImGui::TreeNodeEx(node->name, tree_node_flags)) {

            if (current_node != node_handle && ImGui::IsItemClicked()) {
                current_node.index = node_handle.index;
                current_node.type = node_handle.type;
            }

            for (u32 i = 0; i < node->children.size; i++) {
                imgui_draw_node(node->children[i]);
            }
            ImGui::TreePop();
        }
    }

    void glTFScene::imgui_draw_hierarchy() {
        if (ImGui::Begin("Scene Hierarchy")) {
            ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
            if (ImGui::Button("Add GLTF model", { viewportPanelSize.x, 30 })) {
                char* file_path = nullptr;
                char* filename = nullptr;
                if (file_open_dialog(file_path, filename)) {
                    HDEBUG("Found file!, {}, Oath: {}", filename, file_path);

                    Directory cwd{};
                    directory_current(&cwd);

                    directory_change(file_path);

                    load(filename, file_path, main_allocator, scratch_allocator, loader);

                    directory_change(cwd.path);

                    prepare_draws(renderer, scratch_allocator);

                    //gltf_free(gltf_scene);

                    directory_change(cwd.path);

                    delete[] filename, file_path;
                }
            }

            imgui_draw_node(node_pool.root_node);
            ImGui::End();
        }
    }

    // Nodes //////////////////////////////////////////

    void NodePool::init(Allocator* allocator_) {
        allocator = allocator_;

        mesh_nodes.init(allocator_, 300, sizeof(MeshNode));
        base_nodes.init(allocator_, 50, sizeof(Node));
        light_nodes.init(allocator_, 5, sizeof(LightNode));

        root_node = obtain_node(NodeType::Node);

        Node* root = (Node*)access_node(root_node);
        root->children.init(allocator, 4);
        root->parent = { k_invalid_index, NodeType::Node };
        root->name = "Root_Node";
        root->world_transform = Transform{};
        root->local_transform = Transform{};
    }

    void NodePool::shutdown() {
        mesh_nodes.shutdown();
        base_nodes.shutdown();
        light_nodes.shutdown();
    }

    void* NodePool::access_node(NodeHandle handle) {
        switch (handle.type)
        {
        case NodeType::Node:
            return base_nodes.access_resource(handle.index);
        case NodeType::MeshNode:
            return mesh_nodes.access_resource(handle.index);
        case NodeType::LightNode:
            return light_nodes.access_resource(handle.index);
        default:
            HERROR("Invalid NodeType");
            return nullptr;
        }
    }

    Node* NodePool::get_root_node() {
        Node* root = (Node*)access_node(root_node);
        HASSERT(root);
        return root;
    }

    NodeHandle NodePool::obtain_node(NodeType type) {
        NodeHandle handle{};
        switch (type)
        {
        case NodeType::Node: {
            handle = { base_nodes.obtain_resource(), NodeType::Node };
            Node* base_node = new((Node*)access_node(handle)) Node();
            base_node->updateFunc = update_transform;
            base_node->handle = handle;
            break;
        }
        case NodeType::MeshNode: {
            handle = { mesh_nodes.obtain_resource(), NodeType::MeshNode };
            MeshNode* mesh_node = new((MeshNode*)access_node(handle)) MeshNode();
            mesh_node->updateFunc = update_mesh_transform;
            mesh_node->handle = handle;
            break;
        }
        case NodeType::LightNode: {
            handle = { light_nodes.obtain_resource(), NodeType::LightNode };
            LightNode* light_node = new((LightNode*)access_node(handle)) LightNode();
            light_node->updateFunc = update_transform;
            light_node->handle = handle;
            break;
        }
        default:
            HERROR("Invalid NodeType");
            break;
        }
        return handle;
    }
}// namespace Helix