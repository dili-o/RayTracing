#pragma once

#include "vendor/enkiTS/TaskScheduler.h"

#include "Core/Gltf.hpp"

#include "Renderer/AsynchronousLoader.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/HelixImgui.hpp"
#include "Renderer/GPUProfiler.hpp"
#include "Renderer/FrameGraph.hpp"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <vendor/glm/glm/glm.hpp>
#include <vendor/glm/glm/gtc/quaternion.hpp>
#include <vendor/glm/glm/gtx/quaternion.hpp>


inline f32 random_float() {
    // Returns a random real in [0,1).
    return std::rand() / (RAND_MAX + 1.0);
}

inline f32 random_float(f32 min, f32 max) {
    // Returns a random real in [min,max).
    return min + (max - min) * random_float();
}

inline i32 random_int(i32 min, i32 max) {
    // Returns a random integer in [min,max].
    return i32(random_float(min, max + 1));
}

namespace Helix {
    static const u16    INVALID_TEXTURE_INDEX = 0xffff;

    static const u32    k_material_descriptor_set_index = 1;
    static const u32    k_max_depth_pyramid_levels = 16;

    struct glTFScene;

    struct Transform {

        glm::vec3                   scale = glm::vec3(1.0f);
        glm::quat                   rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3                   translation = glm::vec3(0.0f);

        //void                    reset();
        glm::mat4                   calculate_matrix() const {
            glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), translation);
            glm::mat4 rotation_matrix = glm::toMat4(rotation);
            glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0f), scale);

            glm::mat4 local_matrix = translation_matrix * rotation_matrix * scale_matrix;
            return local_matrix;
        }

        void               set_transform(glm::mat4& model) {
            translation = glm::vec3(model[3]);

            glm::mat3 rotation_matrix = glm::mat3(
                glm::normalize(glm::vec3(model[0])),
                glm::normalize(glm::vec3(model[1])),
                glm::normalize(glm::vec3(model[2]))
            );
            // Convert the rotation matrix to a quaternion
            rotation = glm::quat_cast(rotation_matrix);

            scale.x = glm::length(glm::vec3(model[0]));
            scale.y = glm::length(glm::vec3(model[1]));
            scale.z = glm::length(glm::vec3(model[2]));
        }
    }; // struct Transform

    struct LightUniform {
        glm::mat4       model;
        glm::mat4       view_projection;
        glm::vec4       camera_position_texture_index;
    };

    enum DrawFlags {
        DrawFlags_AlphaMask = 1 << 0,
        DrawFlags_DoubleSided = 1 << 1,
        DrawFlags_Transparent = 1 << 2,
        DrawFlags_Phong = 1 << 3,
        DrawFlags_HasNormals = 1 << 4,
        DrawFlags_HasTexCoords = 1 << 5,
        DrawFlags_HasTangents = 1 << 6,
        DrawFlags_HasJoints = 1 << 7,
        DrawFlags_HasWeights = 1 << 8,
        DrawFlags_AlphaDither = 1 << 9,
        DrawFlags_Cloth = 1 << 10,
    }; // enum DrawFlags

    struct PBRMaterial {
        Material* material;

        BufferHandle        material_buffer;
        DescriptorSetHandle descriptor_set;

        u16                 diffuse_texture_index;
        u16                 roughness_texture_index;
        u16                 normal_texture_index;
        u16                 occlusion_texture_index;

        glm::vec4           base_color_factor;
        glm::vec4           metallic_roughness_occlusion_factor;

        f32                 alpha_cutoff;
        u32                 flags;
    };

    struct Mesh {

        PBRMaterial         pbr_material;

        BufferHandle        index_buffer;
        VkIndexType         index_type;
        u32                 index_offset;

        BufferHandle        position_buffer;
        BufferHandle        tangent_buffer;
        BufferHandle        normal_buffer;
        BufferHandle        texcoord_buffer;

        u32                 position_offset;
        u32                 tangent_offset;
        u32                 normal_offset;
        u32                 texcoord_offset;

        u32                 primitive_count;
        u32                 node_index;

        u32                 meshlet_offset;
        u32                 meshlet_count;

        u32                 gpu_mesh_index = u32_max;

        glm::vec4           bounding_sphere;

        bool                is_transparent() const { return (pbr_material.flags & (DrawFlags_AlphaMask | DrawFlags_Transparent)) != 0; }
        bool                is_double_sided() const { return (pbr_material.flags & DrawFlags_DoubleSided) == DrawFlags_DoubleSided; }
    }; // struct Mesh

    struct MeshInstance {
        Mesh* mesh;
        u32                 material_pass_index;
    }; // struct MeshInstance


    // Gpu Data Structs /////////////////////////////////////////////////////////////////////////
    struct alignas(16) GPUMeshDrawCounts {
        u32                     opaque_mesh_visible_count;
        u32                     opaque_mesh_culled_count;
        u32                     transparent_mesh_visible_count;
        u32                     transparent_mesh_culled_count;

        u32                     total_count;
        u32                     depth_pyramid_texture_index;
        u32                     late_flag;
        u32                     pad001;
    }; // struct GPUMeshDrawCounts Draw count buffer used in indirect draw calls
#if NVIDIA
    struct alignas(16) GPUMeshDrawCommand {
        u32                     mesh_index;
        VkDrawIndexedIndirectCommand indirect; // 5 uint32_t
        VkDrawMeshTasksIndirectCommandNV indirectMS; // 2 uint32_t
    };
#else
    struct alignas(16) GPUMeshDrawCommand {
        u32                     mesh_index;
        u32                     firstTask;
        VkDrawIndexedIndirectCommand indirect; // 5 uint32_t
        VkDrawMeshTasksIndirectCommandEXT indirectMS; // 3 uint32_t
        u32                     padding[2];
    };
        
#endif // NVIDIA

    struct GPUDebugIcon {
        glm::vec4               position_texture_index[5]; // x,y,z for position, w for texture index
        u32                     count; // TODO: Maybe store the texture index as a u16 and the count as a u16
    };

    struct alignas(16) GPUMaterialData {

        u32                     textures[4]; // diffuse, roughness, normal, occlusion
        // PBR
        glm::vec4               emissive; // emissive_color_factor + emissive texture index
        glm::vec4               base_color_factor;
        glm::vec4               metallic_roughness_occlusion_factor; // metallic, roughness, occlusion

        u32                     flags;
        f32                     alpha_cutoff;
        u32                     vertex_offset;
        u32                     mesh_index; // Not used

        u32                     meshlet_offset;
        u32                     meshlet_count;
        u32                     padding0_;
        u32                     padding1_;

        // Phong
        glm::vec4               diffuse_colour;

        glm::vec3               specular_colour;
        f32                     specular_exp;

        glm::vec3               ambient_colour;
        f32                     padding2_;

    }; // struct GpuMaterialData

    struct alignas(16) GPUMeshInstanceData {
        glm::mat4               world;
        glm::mat4               inverse_world;

        u32                     mesh_index;
        u32                     pad000;
        u32                     pad001;
        u32                     pad002;
    }; // struct GpuMeshInstanceData

    struct GPUMeshData {
        glm::mat4           model;
        glm::mat4           inverse_model;

        u32                 textures[4]; // diffuse, roughness, normal, occlusion
        glm::vec4           base_color_factor;
        glm::vec4           metallic_roughness_occlusion_factor; // metallic, roughness, occlusion
        float               alpha_cutoff;
        float               padding_[3];

        u32                 flags;
        float               padding_2[3];
    }; // struct GPUMeshData


    struct alignas(16) GPUMeshlet {

        glm::vec3               center;
        f32                     radius;

        i8                      cone_axis[3];
        i8                      cone_cutoff;

        u32                     data_offset;
        u32                     mesh_index;
        u8                      vertex_count;
        u8                      triangle_count;
    }; // struct GPUMeshlet

    struct GPUMeshletVertexPosition {

        float                   position[3];
        float                   padding;
    }; // struct GPUMeshletVertexPosition

    struct GPUMeshletVertexData {

        u8                      normal[4];
        u8                      tangent[4];
        u16                     uv_coords[2];
        float                   padding;
    }; // struct GPUMeshletVertexData

    struct GPUSceneData {
        glm::mat4               inverse_view;
        glm::mat4               inverse_projection;
        glm::mat4               inverse_view_projection;
        glm::mat4               view_matrix;
        glm::mat4               projection_matrix;
        glm::mat4               previous_view_projection;

        glm::vec4               camera_position;
        glm::vec4               camera_position_debug;
        glm::vec4               light_position;

        u32                     sphere_count;
        u32                     material_count;
        u32                     dither_texture_index;
        f32                     z_near;

        f32                     z_far;
        f32                     projection_00;
        f32                     projection_11;
        u32                     frustum_cull_meshes;

        u32                     frustum_cull_meshlets;
        u32                     occlusion_cull_meshes;
        u32                     occlusion_cull_meshlets;
        u32                     freeze_occlusion_camera;

        f32                     seed_x;
        f32                     seed_y;
        f32                     aspect_ratio;
        f32                     pad0001;

        glm::vec4               frustum_planes[6];

    }; // struct GPUSceneData

    // Gpu Data Structs /////////////////////////////////////////////////////////////////////////

    // Nodes //////////////////////////////////////
    enum class NodeType {
        Node,
        MeshNode,
        LightNode
    };

    struct NodeHandle {
        u32                     index = k_invalid_index;
        NodeType                type = NodeType::Node;

        // Equality operator
        bool operator==(const NodeHandle& other) const {
            return index == other.index && type == other.type;
        }

        // Inequality operator
        bool operator!=(const NodeHandle& other) const {
            return !(*this == other);
        }
    };

    struct Node;

    struct NodePool {

        void                    init(Allocator* allocator);
        void                    shutdown();

        NodeHandle              obtain_node(NodeType type);
        void*                   access_node(NodeHandle handle);
        Node*                   get_root_node();

        Allocator* allocator;

        NodeHandle              root_node;

        ResourcePool            base_nodes;
        ResourcePool            mesh_nodes;
        ResourcePool            light_nodes;
    };

    struct Node {
        NodeHandle              handle = { k_invalid_index, NodeType::Node };
        NodeHandle              parent = { k_invalid_index, NodeType::Node };
        Array<NodeHandle>       children;
        Transform               local_transform{ };
        Transform               world_transform{ };

        cstring                 name = nullptr;

        void                    (*updateFunc)(Node*, NodePool* node_pool);

        void            update_transform(NodePool* node_pool) {
            if (!updateFunc) {
                HWARN("Node does not have update function");
                return;
            }
            updateFunc(this, node_pool);


        }
        void                    add_child(Node* node) {
            node->parent = handle;
            children.push(node->handle);// TODO: Fix array issue!!!!!!!!!!!!
        }
    };

    struct MeshNode : public Node {
        Mesh* mesh; 
    };

    struct LightNode : public Node {
        // Doesn't hold any data for now;
    };

    ///////////////////////////////////////////////
    struct Scene {

        virtual void            init(Renderer* renderer, Allocator* resident_allocator, FrameGraph* frame_graph, StackAllocator* stack_allocator, AsynchronousLoader* async_loader) { };
        virtual void            load(cstring filename, cstring path, Allocator* resident_allocator, StackAllocator* temp_allocator, AsynchronousLoader* async_loader) { };
        virtual void            free_gpu_resources(Renderer* renderer) { };
        virtual void            unload(Renderer* renderer) { };

        virtual void            register_render_passes(FrameGraph* frame_graph) { };
        virtual void            prepare_draws(Renderer* renderer, StackAllocator* stack_allocator) { };

        virtual void            fill_gpu_data_buffers(float model_scale) { };
        virtual void            submit_draw_task(ImGuiService* imgui, GPUProfiler* gpu_profiler, enki::TaskScheduler* task_scheduler) { };

        Array<GPUMeshlet>       meshlets;
        Array<GPUMeshletVertexPosition> meshlets_vertex_positions;
        Array<GPUMeshletVertexData> meshlets_vertex_data;
        Array<u32>              meshlet_vertex_and_index_indices;

        // Gpu buffers
        BufferHandle            material_data_buffer = k_invalid_buffer; // Contains the material data for opaque meshes and transparent meshes
        BufferHandle            mesh_instances_buffer = k_invalid_buffer;
        BufferHandle            mesh_bounds_buffer = k_invalid_buffer;
        BufferHandle            scene_constant_buffer = k_invalid_buffer;
        BufferHandle            meshlets_buffer = k_invalid_buffer;
        BufferHandle            meshlet_vertex_and_index_indices_buffer = k_invalid_buffer;
        BufferHandle            meshlets_vertex_pos_buffer = k_invalid_buffer;
        BufferHandle            meshlets_vertex_data_buffer = k_invalid_buffer;

        // Ray Tracing Data
        BufferHandle            spheres_buffer;
        BufferHandle            materials_buffer;
        BufferHandle            bvh_nodes_buffer;
        BufferHandle            bvh_debug_vertex_buffer;
        BufferHandle            bvh_debug_index_buffer;
        BufferHandle            bvh_debug_instance_buffer;
        BufferHandle            scene_info_buffer;

        // Gpu debug draw
        BufferHandle            debug_line_buffer = k_invalid_buffer;
        BufferHandle            debug_line_count_buffer = k_invalid_buffer;
        BufferHandle            debug_line_indirect_command_buffer = k_invalid_buffer;

        GPUSceneData            scene_data;

        DescriptorSetHandle     mesh_shader_descriptor_set[k_max_frames];

        GPUMeshDrawCounts       mesh_draw_counts;

        Renderer*               renderer = nullptr;

        u32                     fullscreen_texture_index = u32_max;

    }; // struct 

    const f32 INF = std::numeric_limits<f32>::infinity();
    
    struct alignas(16)Interval {
        f32 min = 0.f;
        f32 max = 0.f;
        f32 padding[2];

        Interval() : min(+INF), max(-INF) {}

        Interval(float min, float max) :
            min(min), max(max) {}

        Interval(const Interval& a, const Interval& b) {
            min = a.min <= b.min ? a.min : b.min;
            max = a.max >= b.max ? a.max : b.max;
        }

        static Interval empty() {
            return Interval(+INF, -INF);
        }

        static Interval universe() {
            return Interval(-INF, +INF);
        }

        f32 size() const {
            return max - min;
        }
    };

    struct alignas(16) AABB { // For now AABB is stored in the Spheres 24 bytes
        //Interval x;
        //Interval y;
        //Interval z;
        //f32 padding[12];
        glm::vec4 min;
        glm::vec4 max;

        AABB() {}

        AABB(const Interval& x, const Interval& y, const Interval& z)
            : min(glm::vec4(x.min, y.min, z.min, 1.0f)), 
            max(glm::vec4(x.max, y.max, z.max, 1.0f))
        {

        }

        AABB(const glm::vec3& a, const glm::vec3& b) {
            min = glm::vec4(glm::min(a, b), 1.0f);
            max = glm::vec4(glm::max(a, b), 1.0f);
        }

        AABB(const AABB& a, const AABB& b) {
            glm::vec3 min_vec = glm::min(glm::vec3(a.min), glm::vec3(b.min));
            glm::vec3 max_vec = glm::max(glm::vec3(a.max), glm::vec3(b.max));
            min = glm::vec4(min_vec, 1.0f);
            max = glm::vec4(max_vec, 1.0f);
        }

        const Interval& axis_interval(int n) const {
            Interval x(min.x, max.x);
            Interval y(min.y, max.y);
            Interval z(min.z, max.z);
            if (n == 1) return y;
            if (n == 2) return z;
            return x;
        }

        static AABB empty() {
            return AABB(Interval::empty(), Interval::empty(), Interval::empty());
        }

        static AABB universe() {
            return AABB(Interval::universe(), Interval::universe(), Interval::universe());
        }

        u32 longest_axis() const {
            // Returns the index of the longest axis of the bounding box.

            float extentX = max.x - min.x;
            float extentY = max.y - min.y;
            float extentZ = max.z - min.z;

            // Determine the longest axis
            if (extentX >= extentY && extentX >= extentZ) {
                return 0; // X-axis
            }
            else if (extentY >= extentZ) {
                return 1; // Y-axis
            }
            else {
                return 2; // Z-axis
            }
        }
    };

    namespace MaterialType {
        enum {
            LAMBERTIAN = 1 << 0,
            METAL = 1 << 1,
            DIELECTRIC = 1 << 2,
        };
    }// MaterialType

    struct alignas(16) GPUMaterial {
        glm::vec3       albedo; //12
        u32             type; // 4

        f32             fuzz_ir; // Acts as the refraction_index for non metals // 4
        f32             padding[3] = {}; // 12

        GPUMaterial() = default;

        GPUMaterial(glm::vec3 albedo_, u32 type_, f32 fuzz_ir_ = 1.0f) 
            : albedo(albedo_), type(type_), fuzz_ir(fuzz_ir_) {};
    };

    struct alignas(16) Sphere {
        glm::vec3       origin;
        f32             radius;

        AABB            bounding_box;

        u32             material_index;
        u32             padding[3];

        Sphere(glm::vec3 origin_, f32 radius_, u32 index) 
            : origin(origin_), radius(radius_), material_index(index){

            glm::vec3 radius_v = glm::vec3(radius);
            bounding_box = AABB(origin_ - radius_v, origin_ + radius_v);
        };
    };

    struct alignas(16) BVHNode { // 48 bytes
        u32 sphere_index = 0;
        u32 sphere_count = 0;
        u32 node_child_index = 0;
        u32 padding;

        AABB bounding_box;

        glm::vec4 debug_color;

        BVHNode() 
        : sphere_index(0), sphere_count(0), node_child_index(0), bounding_box(AABB::empty())
        {}

        BVHNode(Array<BVHNode>& bvh_arr, Sphere* spheres, u32 start, u32 end, u32 max_depth)
        : sphere_index(start)
        {
            if (max_depth == 0)
                max_depth++;
            debug_color = glm::vec4(random_float(0.f, 1.f), random_float(0.f, 1.f), random_float(0.f, 1.f), 1.f);

            size_t test = sizeof(BVHNode);

            bounding_box = AABB::empty();
            for (u32 object_index = start; object_index < end; object_index++) {
                bounding_box = AABB(bounding_box, spheres[object_index].bounding_box);
            }

            i32 axis = bounding_box.longest_axis();

            auto comparator = (axis == 0) ? box_x_compare
                            : (axis == 1) ? box_y_compare
                            : box_z_compare;

            u32 object_span = end - start;
            sphere_count = object_span;

            if (object_span <= max_depth) {
                node_child_index = 0;
            }else {
                std::sort(spheres + start, spheres + end, comparator);

                u32 mid = start + object_span / 2;

                node_child_index = bvh_arr.size;

                bvh_arr.push(BVHNode());
                bvh_arr.push(BVHNode());

                bvh_arr[node_child_index] = BVHNode(bvh_arr, spheres, start, mid, max_depth);
                bvh_arr[node_child_index + 1] = BVHNode(bvh_arr, spheres, mid, end, max_depth);
            }
        }

        static bool box_compare(
            const Sphere& a, const Sphere& b, int axis_index
        ) {
            auto a_axis_interval = a.bounding_box.axis_interval(axis_index);
            auto b_axis_interval = b.bounding_box.axis_interval(axis_index);
            return a_axis_interval.min < b_axis_interval.min;
        }

        static bool box_x_compare(const Sphere& a, const Sphere& b) {
            return box_compare(a, b, 0);
        }

        static bool box_y_compare(const Sphere& a, const Sphere& b) {
            return box_compare(a, b, 1);
        }

        static bool box_z_compare(const Sphere& a, const Sphere& b) {
            return box_compare(a, b, 2);
        }
    };

    struct SceneInfo {
        u32 total_bounce_count;
    };

    
    struct RayTracingPass : public FrameGraphRenderPass {
        void                    render(CommandBuffer* gpu_commands, Scene* scene) override;

        void                    prepare_draws(Scene& scene, FrameGraph* frame_graph, Allocator* resident_allocator);
        void                    free_gpu_resources();

        Renderer*               renderer = nullptr;

        PipelineHandle          pipeline_handle;
        DescriptorSetHandle     d_set;
        u32                     output_image_index;
        TextureHandle           accumulated_image_handle;
        u32                     frame_index = 1;
        bool                    camera_moved = false;
        u32                     node_index;
        u32                     pad = 0;
        u32                     bvh_count;

    }; // struct RayTracingPass

    struct DebugBVHPass : public FrameGraphRenderPass {
        void                    render(CommandBuffer* gpu_commands, Scene* scene) override;

        void                    prepare_draws(Scene& scene, FrameGraph* frame_graph, Allocator* resident_allocator);
        void                    free_gpu_resources();

        Renderer* renderer = nullptr;

        PipelineHandle          pipeline_handle;
        DescriptorSetHandle     d_set;
        u32                     bvh_count;

    }; // struct DebugBVHPass

    struct glTFScene : public Scene {

        void                    init(Renderer* renderer, Allocator* resident_allocator, FrameGraph* frame_graph, StackAllocator* stack_allocator, AsynchronousLoader* async_loader) override;
        void                    load(cstring filename, cstring path, Allocator* resident_allocator, StackAllocator* temp_allocator, AsynchronousLoader* async_loader) override {};
        void                    free_gpu_resources(Renderer* renderer) override;
        void                    unload(Renderer* renderer) override;

        void                    register_render_passes(FrameGraph* frame_graph) override;
        void                    prepare_draws(Renderer* renderer, StackAllocator* stack_allocator) override;
        void                    fill_pbr_material(Renderer& renderer, glTF::Material& material, PBRMaterial& pbr_material);
        u16                     get_material_texture(GpuDevice& gpu, glTF::TextureInfo* texture_info);
        u16                     get_material_texture(GpuDevice& gpu, i32 gltf_texture_index);

        void                    fill_gpu_data_buffers(float model_scale) override;
        void                    submit_draw_task(ImGuiService* imgui, GPUProfiler* gpu_profiler, enki::TaskScheduler* task_scheduler) override;

        void                    draw_mesh(CommandBuffer* gpu_commands, Mesh& mesh);

        void                    destroy_node(NodeHandle handle);

        void                    imgui_draw_node(NodeHandle node_handle);
        void                    imgui_draw_hierarchy();

        void                    imgui_draw_node_property(NodeHandle node_handle);

        Array<Mesh>             opaque_meshes;
        Array<Mesh>             transparent_meshes;

        // All graphics resources used by the scene
        Array<TextureResource>  images; // TODO: Maybe just store the pool index rather than the whole Texture resource
        Array<SamplerResource>  samplers;
        Array<BufferResource>   buffers;

        glm::vec4 tester = glm::vec4(1.0f); // TODO: Remove

        u32                     current_images_count = 0;
        u32                     current_buffers_count = 0;
        u32                     current_samplers_count = 0;


        glTF::glTF              gltf_scene; // Source gltf scene

        NodePool                node_pool;

        RayTracingPass          ray_tracing_pass;
        DebugBVHPass            debug_bvh_pass;

        // Fullscreen data
        Program*                fullscreen_program = nullptr;
        DescriptorSetHandle     fullscreen_ds;

        NodeHandle              current_node{ };

        FrameGraph*             frame_graph;
        StackAllocator*         scratch_allocator;
        Allocator*              main_allocator;
        AsynchronousLoader*     loader;
        Material*               pbr_material;
        StringBuffer            names;


        // Buffers
        BufferHandle            light_cb;
        TextureResource         light_texture;

    }; // struct GltfScene

    struct glTFDrawTask : public enki::ITaskSet {

        GpuDevice*          gpu = nullptr;
        FrameGraph*         frame_graph = nullptr;
        Renderer*           renderer = nullptr;
        ImGuiService*       imgui = nullptr;
        GPUProfiler*        gpu_profiler = nullptr;
        glTFScene*          scene = nullptr;
        u32                 thread_id = 0;

        void                init(GpuDevice* gpu_, FrameGraph* frame_graph_, Renderer* renderer_, ImGuiService* imgui_, GPUProfiler* gpu_profiler_, glTFScene* scene_);

        void                ExecuteRange(enki::TaskSetPartition range_, u32 threadnum_) override;

    }; // struct DrawTask



} // namespace Helix

