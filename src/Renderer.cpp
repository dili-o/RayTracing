#include "Renderer.hpp"
#include "Core/Assert.hpp"
#include "Core/Defines.hpp"
#include "Core/Exceptions.hpp"
#include "Material.hpp"
#include "Vulkan/VkDeviceManager.h"
#include "Vulkan/VkResourceManager.hpp"
#include "Vulkan/VkShaderCompilation.h"
#include "Vulkan/VkStagingBuffer.h"
#include "Vulkan/VkUtils.hpp"
// Vendor
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <stb_image.h>
#include <tracy/public/tracy/Tracy.hpp>

// TODO: Make configurable
constexpr u32 samples_per_pixel = 3u;

static constexpr VkFormat output_image_format = VK_FORMAT_R32G32B32A32_SFLOAT;
static constexpr size_t MAX_TRIANGLE_COUNT = 4'000'000;
static constexpr size_t MAX_MATERIAL_COUNT = 1'000;
static constexpr size_t MAX_BLAS_COUNT = 4'000;
static constexpr u32 BYTES_PER_PIXEL = 4u;

namespace hlx {
struct alignas(16) UniformData {
  glm::vec4 pixel00_loc;
  glm::vec4 pixel_delta_u;
  glm::vec4 pixel_delta_v;
  glm::vec4 camera_center;
  VkDeviceAddress triangle_geom_buffer;
  VkDeviceAddress triangle_shading_buffer;
  VkDeviceAddress tlas_nodes_buffer;
  VkDeviceAddress bvh_nodes_buffer;
  VkDeviceAddress blas_buffer;
  VkDeviceAddress blas_instances_buffer;
  VkDeviceAddress tri_ids_buffer;
  VkDeviceAddress lambert_materials_buffer;
  VkDeviceAddress metal_materials_buffer;
  VkDeviceAddress dielectric_materials_buffer;
  VkDeviceAddress emissive_materials_buffer;
};

struct PushConstant {
  VkDeviceAddress uniform_data_buffer;

  u32 image_width;
  u32 image_height;

  u32 triangle_count;
  u32 frame_index;

  f32 pixel_sample_scale;
  f32 recip_sqrt_spp;

  u32 sqrt_spp;
  f32 padding;
};

void generate_sphere(std::vector<glm::vec3> &out_vertices,
                     std::vector<uint32_t> &out_indices,
                     std::vector<glm::vec3> &out_normals,
                     std::vector<glm::vec2> &out_uvs, float radius,
                     uint32_t segments, uint32_t rings,
                     const glm::vec3 &center = glm::vec3(0.f)) {
  uint32_t vertex_offset = static_cast<uint32_t>(out_vertices.size());
  for (uint32_t y = 0; y <= rings; ++y) {
    float v = float(y) / rings;
    float theta = v * glm::pi<float>();

    for (uint32_t x = 0; x <= segments; ++x) {
      float u = float(x) / segments;
      float phi = u * glm::two_pi<float>();

      float sin_theta = sin(theta);
      float cos_theta = cos(theta);
      float sin_phi = sin(phi);
      float cos_phi = cos(phi);

      glm::vec3 normal(sin_theta * cos_phi, cos_theta, sin_theta * sin_phi);
      glm::vec3 position = center + radius * normal;

      out_vertices.push_back(position);
      out_normals.push_back(normal);
      out_uvs.push_back(glm::vec2(u, v));
    }
  }

  for (uint32_t y = 0; y < rings; ++y) {
    for (uint32_t x = 0; x < segments; ++x) {
      uint32_t i0 = y * (segments + 1) + x;
      uint32_t i1 = i0 + segments + 1;

      out_indices.push_back(i0 + vertex_offset);
      out_indices.push_back(i1 + vertex_offset);
      out_indices.push_back(i0 + 1 + vertex_offset);

      out_indices.push_back(i0 + 1 + vertex_offset);
      out_indices.push_back(i1 + vertex_offset);
      out_indices.push_back(i1 + 1 + vertex_offset);
    }
  }
}

void generate_plane(std::vector<glm::vec3> &out_vertices,
                    std::vector<uint32_t> &out_indices,
                    std::vector<glm::vec3> &out_normals,
                    std::vector<glm::vec2> &out_uvs, float width, float depth,
                    uint32_t x_segments, uint32_t z_segments,
                    const glm::vec3 &center = glm::vec3(0.f)) {
  uint32_t vertex_offset = static_cast<uint32_t>(out_vertices.size());

  for (uint32_t z = 0; z <= z_segments; ++z) {
    float vz = float(z) / z_segments;
    float pos_z = (vz - 0.5f) * depth;

    for (uint32_t x = 0; x <= x_segments; ++x) {
      float ux = float(x) / x_segments;
      float pos_x = (ux - 0.5f) * width;

      out_vertices.push_back(center + glm::vec3(pos_x, 0.f, pos_z));
      out_normals.push_back(glm::vec3(0.f, 1.f, 0.f));
      out_uvs.push_back(glm::vec2(ux, vz));
    }
  }

  for (uint32_t z = 0; z < z_segments; ++z) {
    for (uint32_t x = 0; x < x_segments; ++x) {
      uint32_t i0 = z * (x_segments + 1) + x;
      uint32_t i1 = i0 + x_segments + 1;

      out_indices.push_back(i0 + vertex_offset);
      out_indices.push_back(i1 + vertex_offset);
      out_indices.push_back(i0 + 1 + vertex_offset);

      out_indices.push_back(i0 + 1 + vertex_offset);
      out_indices.push_back(i1 + vertex_offset);
      out_indices.push_back(i1 + 1 + vertex_offset);
    }
  }
}

void generate_cube(std::vector<glm::vec3> &out_vertices,
                   std::vector<uint32_t> &out_indices,
                   std::vector<glm::vec3> &out_normals,
                   std::vector<glm::vec2> &out_uvs, const glm::vec3 &center,
                   float width, float height, float depth) {
  float hx = width * 0.5f;
  float hy = height * 0.5f;
  float hz = depth * 0.5f;

  auto add_face = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                      glm::vec3 normal) {
    uint32_t base = static_cast<uint32_t>(out_vertices.size());

    out_vertices.push_back(v0);
    out_vertices.push_back(v1);
    out_vertices.push_back(v2);
    out_vertices.push_back(v3);

    out_normals.push_back(normal);
    out_normals.push_back(normal);
    out_normals.push_back(normal);
    out_normals.push_back(normal);

    out_uvs.push_back(glm::vec2(0.f, 0.f));
    out_uvs.push_back(glm::vec2(1.f, 0.f));
    out_uvs.push_back(glm::vec2(1.f, 1.f));
    out_uvs.push_back(glm::vec2(0.f, 1.f));

    out_indices.push_back(0 + base);
    out_indices.push_back(1 + base);
    out_indices.push_back(2 + base);

    out_indices.push_back(0 + base);
    out_indices.push_back(2 + base);
    out_indices.push_back(3 + base);
  };

  // Front (+Z)
  add_face(center + glm::vec3(-hx, -hy, hz), center + glm::vec3(hx, -hy, hz),
           center + glm::vec3(hx, hy, hz), center + glm::vec3(-hx, hy, hz),
           glm::vec3(0, 0, 1));

  // Back (-Z)
  add_face(center + glm::vec3(hx, -hy, -hz), center + glm::vec3(-hx, -hy, -hz),
           center + glm::vec3(-hx, hy, -hz), center + glm::vec3(hx, hy, -hz),
           glm::vec3(0, 0, -1));

  // Left (-X)
  add_face(center + glm::vec3(-hx, -hy, -hz), center + glm::vec3(-hx, -hy, hz),
           center + glm::vec3(-hx, hy, hz), center + glm::vec3(-hx, hy, -hz),
           glm::vec3(-1, 0, 0));

  // Right (+X)
  add_face(center + glm::vec3(hx, -hy, hz), center + glm::vec3(hx, -hy, -hz),
           center + glm::vec3(hx, hy, -hz), center + glm::vec3(hx, hy, hz),
           glm::vec3(1, 0, 0));

  // Top (+Y)
  add_face(center + glm::vec3(-hx, hy, hz), center + glm::vec3(hx, hy, hz),
           center + glm::vec3(hx, hy, -hz), center + glm::vec3(-hx, hy, -hz),
           glm::vec3(0, 1, 0));

  // Bottom (-Y)
  add_face(center + glm::vec3(-hx, -hy, -hz), center + glm::vec3(hx, -hy, -hz),
           center + glm::vec3(hx, -hy, hz), center + glm::vec3(-hx, -hy, hz),
           glm::vec3(0, -1, 0));
}

void Renderer::init(VkDeviceManager *p_device, VkResourceManager *p_rm,
                    u32 output_image_width, u32 output_image_height) {
  HASSERT(p_device);
  HASSERT(p_rm);
  this->p_device = p_device;
  this->p_rm = p_rm;

  blas_use_count.resize(MAX_BLAS_COUNT);
  blas_use_count.assign(MAX_BLAS_COUNT, 0);

  // Create staging buffer
  staging_buffer.init(
      p_device, p_rm,
      p_device->queue_family_indices.transfer_family_index.value(),
      p_device->vk_transfer_queue, 4096 * 4096 * 4);

  // Create the output image
  create_output_image(output_image_width, output_image_height);

  // Create descriptor set layout
  VkDescriptorSetLayoutBinding output_image_binding{};
  output_image_binding.binding = 0;
  output_image_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  output_image_binding.descriptorCount = 1;
  output_image_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutCreateInfo layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layout_info.bindingCount = 1;
  layout_info.pBindings = &output_image_binding;
  set_layout =
      p_rm->create_descriptor_set_layout("PathTracingSetLayout", layout_info);
  const VkDescriptorSetLayout vk_set_layout =
      p_rm->access_set_layout(set_layout)->vk_handle;

  // Allocate the set
  const VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = p_device->vk_descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &vk_set_layout};
  VK_CHECK(vkAllocateDescriptorSets(p_device->vk_device, &alloc_info, &vk_set));

  // Update the set
  const VkDescriptorImageInfo image_update_info{
      .sampler = VK_NULL_HANDLE,
      .imageView = p_rm->access_image_view(output_image_view)->vk_handle,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

  VkWriteDescriptorSet write_info = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = vk_set,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .pImageInfo = &image_update_info};

  vkUpdateDescriptorSets(p_device->vk_device, 1, &write_info, 0, nullptr);

  // Create buffers
  VmaAllocationCreateInfo vma_alloc_info{
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
  VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer_info.size = MAX_TRIANGLE_COUNT * sizeof(TriangleGeom);
  buffer_info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  triangle_geom_buffer =
      p_rm->create_buffer("TriangleGeomBuffer", buffer_info, vma_alloc_info);
  tri_geom_data = static_cast<TriangleGeom *>(malloc(buffer_info.size));

  buffer_info.size = MAX_TRIANGLE_COUNT * sizeof(TriangleShading);
  triangle_shading_buffer =
      p_rm->create_buffer("TriangleShadingBuffer", buffer_info, vma_alloc_info);
  tri_surface_data = static_cast<TriangleShading *>(malloc(buffer_info.size));
  triangle_centroids_data =
      static_cast<glm::vec3 *>(malloc(sizeof(glm::vec3) * MAX_TRIANGLE_COUNT));

  buffer_info.size = MAX_TRIANGLE_COUNT * sizeof(u32);
  tri_ids_buffer =
      p_rm->create_buffer("TriangleIDsBuffer", buffer_info, vma_alloc_info);
  tri_id_allocator.init(buffer_info.size, alignof(u32));

  // Material buffers
  lambert_mats.init(MAX_MATERIAL_COUNT, p_rm);
  metal_mats.init(MAX_MATERIAL_COUNT, p_rm);
  dielectric_mats.init(MAX_MATERIAL_COUNT, p_rm);
  emissive_mats.init(MAX_MATERIAL_COUNT, p_rm);

  // Create with upper bound limit
  buffer_info.size = (MAX_TRIANGLE_COUNT * 2 - 1) * sizeof(BVHNode);
  bvh_nodes_buffer =
      p_rm->create_buffer("BVHNodesBuffer", buffer_info, vma_alloc_info);
  bvh_nodes_allocator.init(buffer_info.size, sizeof(BVHNode));

  // Initialize blases_index_pool, blas_buffer and blases vector
  blases_index_pool.init(MAX_BLAS_COUNT);
  buffer_info.size = MAX_BLAS_COUNT * sizeof(BLAS);
  blas_buffer = p_rm->create_buffer("BLASBuffer", buffer_info, vma_alloc_info);
  blases.resize(MAX_BLAS_COUNT);

  // Initialize blas_inst_index_pool, blas_instances_buffer and blas_instances
  // vector
  blas_inst_index_pool.init(MAX_BLAS_COUNT);
  buffer_info.size = MAX_BLAS_COUNT * sizeof(BLASInstance);
  blas_instances_buffer =
      p_rm->create_buffer("BLASInstancesBuffer", buffer_info, vma_alloc_info);
  blas_instances.resize(MAX_BLAS_COUNT);

  // TLAS buffer
  buffer_info.size = MAX_BLAS_COUNT * sizeof(TLASNode);
  tlas_nodes_buffer =
      p_rm->create_buffer("TLASNodesBuffer", buffer_info, vma_alloc_info);

  // Load primitive data
  load_plane_data();
  load_cube_data();
  load_sphere_data();

  // Uniform buffers
  buffer_info.size = sizeof(UniformData);
  buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  vma_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  for (BufferHandle &handle : uniform_buffers) {
    handle = p_rm->create_buffer("UniformBuffer", buffer_info, vma_alloc_info);
  }

  // Create sampler
  VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sampler_info.magFilter = VK_FILTER_NEAREST;
  sampler_info.minFilter = VK_FILTER_NEAREST;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.anisotropyEnable = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.minLod = 0.f;
  sampler_info.maxLod = VK_LOD_CLAMP_NONE;
  sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  texture_sampler = p_rm->create_sampler("TextureSampler", sampler_info);

  // Create path tracing shader
  ShaderHandle path_tracing_shader;
  try {
    ShaderBlob blob;
    VkCompileOptions opts;
    opts.add("ALBEDO_TEXTURE_COUNT", MAX_MATERIAL_COUNT);
    SlangCompiler::compile_code("compute_main", "RayTracing",
                                SHADER_PATH "RayTracing.slang", blob, opts);
    path_tracing_shader = p_rm->create_shader("PathTracingComp", blob);
  } catch (Exception exception) {
    HERROR("{}", exception.what());
  }

  VkPipelineShaderStageCreateInfo shader_stage_info{
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shader_stage_info.module =
      p_rm->access_shader(path_tracing_shader)->vk_handle;
  shader_stage_info.pName = "main";

  VkPushConstantRange push_constant = {.stageFlags =
                                           VK_SHADER_STAGE_COMPUTE_BIT,
                                       .offset = 0,
                                       .size = sizeof(PushConstant)};
  VkDescriptorSetLayout set_layouts[] = {vk_set_layout,
                                         lambert_mats.vk_descriptor_set_layout};
  VkPipelineLayoutCreateInfo pipeline_layout_info{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipeline_layout_info.setLayoutCount = 2;
  pipeline_layout_info.pSetLayouts = set_layouts;
  pipeline_layout_info.pushConstantRangeCount = 1;
  pipeline_layout_info.pPushConstantRanges = &push_constant;

  VkComputePipelineCreateInfo pipelien_create_info{
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  pipelien_create_info.stage = shader_stage_info;
  path_tracing_pipeline = p_rm->create_compute_pipeline(
      "PathTracingPipeline", pipelien_create_info, pipeline_layout_info);

  p_rm->queue_destroy({path_tracing_shader});

  // Create default material
  default_material = add_lambert_material(glm::vec3(0.7f));
  ++lambert_mats.reference_counts[default_material.index];

  // TODO: Remove
  staging_buffer.flush();
  sky_renderer.init(p_rm, texture_sampler);
}

void Renderer::shutdown() {
  HASSERT(p_rm);

  sky_renderer.shutdown(p_rm);
  remove_material(default_material);

  lambert_mats.shutdown(p_rm);
  metal_mats.shutdown(p_rm);
  dielectric_mats.shutdown(p_rm);
  emissive_mats.shutdown(p_rm);

  for (BufferHandle &handle : uniform_buffers) {
    p_rm->queue_destroy({handle});
  }
  p_rm->queue_destroy({tri_ids_buffer});
  p_rm->queue_destroy({blas_instances_buffer});
  p_rm->queue_destroy({blas_buffer});
  p_rm->queue_destroy({tlas_nodes_buffer});
  p_rm->queue_destroy({bvh_nodes_buffer});
  p_rm->queue_destroy({triangle_shading_buffer});
  p_rm->queue_destroy({triangle_geom_buffer});
  p_rm->queue_destroy({set_layout});
  p_rm->queue_destroy({output_image_view});
  p_rm->queue_destroy({path_tracing_pipeline});
  p_rm->queue_destroy({texture_sampler});
  staging_buffer.shutdown();

  for (const auto &[blas_id, allocation] : blas_allocations_map) {
    tri_id_allocator.deallocate(allocation.tri_id_allocation);
    bvh_nodes_allocator.deallocate(allocation.bvh_nodes_allocation);
  }

  bvh_nodes_allocator.shutdown();
  tri_id_allocator.shutdown();
  free(tri_geom_data);
  free(tri_surface_data);
  free(triangle_centroids_data);

  blases_index_pool.release(sphere_blas_index);
  blases_index_pool.release(cube_blas_index);
  blases_index_pool.release(plane_blas_index);
  blases_index_pool.shutdown();

  // TODO: Users of add_blas_instance should be responsible for releasing each
  // blas_instance_index, rather than doing a release_all() here
  blas_inst_index_pool.release_all();
  blas_inst_index_pool.shutdown();

  p_rm = nullptr;
  p_device = nullptr;
}

void Renderer::resize(u32 output_image_width, u32 output_image_height) {
  // delete the old output image
  p_rm->queue_destroy({output_image_view});

  // Recreate the output image
  create_output_image(output_image_width, output_image_height);

  // Update the descriptor set
  const VkDescriptorImageInfo image_update_info{
      .sampler = VK_NULL_HANDLE,
      .imageView = p_rm->access_image_view(output_image_view)->vk_handle,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

  VkWriteDescriptorSet write_info = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = vk_set,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .pImageInfo = &image_update_info};

  vkUpdateDescriptorSets(p_device->vk_device, 1, &write_info, 0, nullptr);
  frame_index = 0;
}

void Renderer::render(Camera &camera) {
  ZoneScoped;
  // Reset frame number if cam has moved
  if (camera.changed) {
    frame_index = 0;
    camera.changed = false;
  }

  lambert_mats.update(p_device);
  // Rebuild tlas if a change was made
  if (rebuild_tlas)
    build_tlas();

  VkCommandBuffer cmd = p_device->get_current_cmd_buffer();

  // Update uniforms
  VulkanImageView *vk_output_image_view =
      p_rm->access_image_view(output_image_view);
  VulkanImage *vk_output_image =
      p_rm->access_image(vk_output_image_view->image_handle);
  VkExtent2D screen_extents = {vk_output_image->width(),
                               vk_output_image->height()};
  f32 focal_length = glm::length(camera.position - camera.look_at);
  f32 theta = degrees_to_radians(camera.fov);
  f32 h = std::tan(theta / 2.f);
  f32 viewport_height = 2.f * h * focal_length;
  f32 viewport_width =
      viewport_height *
      (static_cast<f32>(screen_extents.width) / screen_extents.height);
  // TODO: Move this into the camera's update function
  // Calculate the u,v,w unit basis vectors for the camera coordinate frame.
  camera.w = glm::normalize(camera.position - camera.look_at);
  camera.u = glm::normalize(glm::cross(camera.v_up, camera.w));
  camera.v = glm::cross(camera.w, camera.u);

  // Calculate the vectors across the horizontal and down the vertical viewport
  // edges.
  glm::vec3 viewport_u = viewport_width * camera.u;
  glm::vec3 viewport_v = viewport_height * -camera.v;

  glm::vec3 pixel_delta_u = viewport_u / static_cast<f32>(screen_extents.width);
  glm::vec3 pixel_delta_v =
      viewport_v / static_cast<f32>(screen_extents.height);

  glm::vec3 viewport_upper_left = camera.position - (focal_length * camera.w) -
                                  0.5f * (viewport_u + viewport_v);
  UniformData uniform_data = {
      .pixel00_loc = glm::vec4(
          viewport_upper_left + 0.5f * (pixel_delta_u + pixel_delta_v), 1.f),
      .pixel_delta_u = glm::vec4(pixel_delta_u, 1.f),
      .pixel_delta_v = glm::vec4(pixel_delta_v, 1.f),
      .camera_center = glm::vec4(camera.position, 1.f),
      .triangle_geom_buffer =
          p_rm->access_buffer(triangle_geom_buffer)->vk_device_address,
      .triangle_shading_buffer =
          p_rm->access_buffer(triangle_shading_buffer)->vk_device_address,
      .tlas_nodes_buffer =
          p_rm->access_buffer(tlas_nodes_buffer)->vk_device_address,
      .bvh_nodes_buffer =
          p_rm->access_buffer(bvh_nodes_buffer)->vk_device_address,
      .blas_buffer = p_rm->access_buffer(blas_buffer)->vk_device_address,
      .blas_instances_buffer =
          p_rm->access_buffer(blas_instances_buffer)->vk_device_address,
      .tri_ids_buffer = p_rm->access_buffer(tri_ids_buffer)->vk_device_address,
      .lambert_materials_buffer =
          p_rm->access_buffer(lambert_mats.buffer)->vk_device_address,
      .metal_materials_buffer =
          p_rm->access_buffer(metal_mats.buffer)->vk_device_address,
      .dielectric_materials_buffer =
          p_rm->access_buffer(dielectric_mats.buffer)->vk_device_address,
      .emissive_materials_buffer =
          p_rm->access_buffer(emissive_mats.buffer)->vk_device_address,
  };
  VulkanBuffer *uniform_buffer =
      p_rm->access_buffer(uniform_buffers.at(p_device->current_frame));
  std::memcpy(uniform_buffer->p_data, &uniform_data, sizeof(UniformData));

  // Sky render pass
  sky_renderer.render(p_rm, camera, frame_index);
  push_debug_label(cmd, "Compute Pass");

  // Transition output image to compute write bit
  VkImageMemoryBarrier2 image_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_barrier.subresourceRange.baseMipLevel = 0;
  image_barrier.subresourceRange.levelCount = 1;
  image_barrier.subresourceRange.baseArrayLayer = 0;
  image_barrier.subresourceRange.layerCount = 1;
  image_barrier.image = vk_output_image->vk_handle;
  image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  image_barrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  image_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency_info.dependencyFlags = 0;
  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers = &image_barrier;
  // Clear the image if something has changed
  if (frame_index == 0) {
    // Trasition to transfer dst layout
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_CLEAR_BIT;
    image_barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkCmdPipelineBarrier2(cmd, &dependency_info);

    const VkClearColorValue col = {.float32 = {0.f, 0.f, 0.f, 0.f}};
    const VkImageSubresourceRange range = {.aspectMask =
                                               VK_IMAGE_ASPECT_COLOR_BIT,
                                           .baseMipLevel = 0,
                                           .levelCount = 1,
                                           .baseArrayLayer = 0,
                                           .layerCount = 1};
    vkCmdClearColorImage(cmd, vk_output_image->vk_handle,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &col, 1, &range);

    // Set the src for transitioning to compute
    image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_CLEAR_BIT;
    image_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  }

  image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  image_barrier.dstAccessMask =
      VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT;
  image_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

  vkCmdPipelineBarrier2(cmd, &dependency_info);

  const VulkanPipeline *pipeline = p_rm->access_pipeline(path_tracing_pipeline);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->vk_handle);

  // Ray tracing parameters
  PushConstant push_constant;
  push_constant.image_width = screen_extents.width;
  push_constant.image_height = screen_extents.height;
  push_constant.frame_index = frame_index;
  push_constant.triangle_count = total_triangle_count;
  push_constant.uniform_data_buffer = uniform_buffer->vk_device_address;
  push_constant.sqrt_spp = u32(std::sqrt(samples_per_pixel));
  push_constant.pixel_sample_scale =
      1.f / (push_constant.sqrt_spp * push_constant.sqrt_spp);
  push_constant.recip_sqrt_spp = 1.f / push_constant.sqrt_spp;
  vkCmdPushConstants(cmd, pipeline->vk_pipeline_layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant),
                     &push_constant);
  VkDescriptorSet vk_sets[] = {vk_set, lambert_mats.vk_descriptor_set};
  const VkBindDescriptorSetsInfo bind_info{
      .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      .layout = pipeline->vk_pipeline_layout,
      .firstSet = 0,
      .descriptorSetCount = 2,
      .pDescriptorSets = vk_sets,
  };
  vkCmdBindDescriptorSets2(cmd, &bind_info);

  constexpr u32 thread_group_size = 32;
  vkCmdDispatch(
      cmd,
      (vk_output_image->width() + thread_group_size - 1) / thread_group_size,
      (vk_output_image->height() + thread_group_size - 1) / thread_group_size,
      1);

  pop_debug_label(cmd);
  ++frame_index;
} // namespace hlx

void Renderer::create_output_image(u32 width, u32 height) {
  VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = output_image_format;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo vma_alloc_info{};
  vma_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = image_info.format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = image_info.mipLevels;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  output_image_view = p_rm->create_image_view(
      "OutputImageView", "OutputImage", image_info, vma_alloc_info, view_info);
}

MaterialHandle Renderer::add_lambert_material(i32 width, i32 height,
                                              u8 *pixels) {
  return lambert_mats.add_material(staging_buffer, p_rm, width, height, pixels);
}

MaterialHandle Renderer::add_lambert_material(const glm::vec3 &albedo) {
  glm::vec3 clamped_albedo = glm::clamp(albedo, 0.f, 1.f);
  // Create pixel data
  u8 *pixels = static_cast<u8 *>(malloc(sizeof(u8) * 4));
  HASSERT(pixels);
  pixels[0] = static_cast<u8>(clamped_albedo.x * 255.f);
  pixels[1] = static_cast<u8>(clamped_albedo.y * 255.f);
  pixels[2] = static_cast<u8>(clamped_albedo.z * 255.f);
  pixels[3] = 255;
  MaterialHandle handle = add_lambert_material(1, 1, pixels);
  free(pixels);
  return handle;
}

MaterialHandle Renderer::add_lambert_material(std::string_view file_path) {
  i32 comp, image_width, image_height;
  stbi_set_flip_vertically_on_load(false);
  u8 *raw_bdata = stbi_load(file_path.data(), &image_width, &image_height,
                            &comp, BYTES_PER_PIXEL);
  HASSERT_MSGS(raw_bdata, "Failed to load image: {}", file_path.data());

  MaterialHandle handle =
      add_lambert_material(image_width, image_height, raw_bdata);
  free(raw_bdata);
  return handle;
}

MaterialHandle Renderer::add_metal_material(const glm::vec3 &albedo,
                                            const f32 fuzz) {
  return metal_mats.add_material(staging_buffer, albedo, fuzz);
}

MaterialHandle Renderer::add_dielectric_material(const f32 refractive_index) {
  return dielectric_mats.add_material(staging_buffer, refractive_index);
}

MaterialHandle Renderer::add_emissive_material(const glm::vec3 &intensity) {
  return emissive_mats.add_material(staging_buffer, intensity);
}

void Renderer::remove_material(const MaterialHandle &material_handle) {
  // TODO: Check if any blas instance uses the material
  switch (material_handle.type) {
  case MaterialType::LAMBERT: {
    lambert_mats.remove_material(material_handle, p_rm);
    break;
  }
  case MaterialType::METAL: {
    metal_mats.remove_material(material_handle);
    break;
  }
  case MaterialType::EMISSIVE: {
    emissive_mats.remove_material(material_handle);
    break;
  }
  case MaterialType::DIELECTRIC: {
    dielectric_mats.remove_material(material_handle);
    break;
  }
  default:
    HASSERT_MSG(false, "Renderer::remove_material() - Unknown MaterialType!.");
    break;
  }
}

u32 Renderer::add_blas(std::span<glm::vec3> positions,
                       std::span<glm::vec3> normals, std::span<glm::vec2> uvs,
                       std::span<u32> indices) {
  u32 trig_count = indices.size() / 3;

  // Allocate tri ids data
  void *p_tri_ids =
      tri_id_allocator.allocate(sizeof(u32) * trig_count, sizeof(u32));
  std::ptrdiff_t byte_offset = static_cast<char *>(p_tri_ids) -
                               static_cast<char *>(tri_id_allocator.memory);
  // Get the index into the tri ids pool
  u32 tri_id_index = byte_offset / sizeof(u32);

  // Load the triangle data
  u32 *tri_ids_data = static_cast<u32 *>(p_tri_ids);
  u32 tri_index = tri_id_index;
  u32 index = 0;
  for (size_t i = 0; i < indices.size(); i += 3) {
    tri_geom_data[tri_index] =
        (TriangleGeom(positions[indices[i]], positions[indices[i + 1]],
                      positions[indices[i + 2]]));
    tri_surface_data[tri_index] = (TriangleShading(
        normals[indices[i]], normals[indices[i + 1]], normals[indices[i + 2]],
        uvs[indices[i]], uvs[indices[i + 1]], uvs[indices[i + 2]]));
    triangle_centroids_data[tri_index] =
        ((positions[indices[i]] + positions[indices[i + 1]] +
          positions[indices[i + 2]]) *
         0.3333f);
    tri_ids_data[index] = (tri_id_index + index);
    ++tri_index;
    ++index;
  }

  // Stage triangle data
  {
    std::span<TriangleGeom> data_view =
        std::span(tri_geom_data + tri_id_index, trig_count);
    staging_buffer.stage(data_view.data(), triangle_geom_buffer,
                         tri_id_index * sizeof(TriangleGeom),
                         trig_count * sizeof(TriangleGeom));
  }
  {
    std::span<TriangleShading> data_view =
        std::span(tri_surface_data + tri_id_index, trig_count);
    staging_buffer.stage(data_view.data(), triangle_shading_buffer,
                         tri_id_index * sizeof(TriangleShading),
                         trig_count * sizeof(TriangleShading));
  }

  // Create blas
  u32 prev_blas_nodes_count = bvh_nodes_size;
  // Create vector of bvh_nodes at the upper bound
  std::vector<BVHNode> bvh_nodes(trig_count * 2 - 1);

  u32 blas_index = blases_index_pool.obtain_new();
  BLAS &blas = blases[blas_index];
  // TODO: Right now we create a separate bvh_nodes vector that is used to
  // build the blas. We then allocate the fitted size from the
  // bvh_nodes_allocator and update the blas' bvh_nodes_offset. This means we
  // don't need to pass in a bvh_nodes_offset parameter
  Clock clock;
  clock.start();
  blas.build(bvh_nodes, /*This is redundant*/ prev_blas_nodes_count,
             std::span(tri_geom_data, MAX_TRIANGLE_COUNT),
             std::span(triangle_centroids_data, MAX_TRIANGLE_COUNT),
             std::span(static_cast<u32 *>(tri_id_allocator.memory),
                       MAX_TRIANGLE_COUNT),
             trig_count, tri_id_index);
  HINFO("BLAS build time: {}s", clock.get_elapsed_time_s());

  // Allocate from the bvh_nodes_allocator and copy the data
  void *p_bvh_nodes = bvh_nodes_allocator.allocate(
      sizeof(BVHNode) * blas.nodes_count, sizeof(BVHNode));
  std::memcpy(p_bvh_nodes, bvh_nodes.data(),
              sizeof(BVHNode) * blas.nodes_count);
  byte_offset = static_cast<char *>(p_bvh_nodes) -
                static_cast<char *>(bvh_nodes_allocator.memory);
  HASSERT((byte_offset % sizeof(BVHNode)) == 0);
  blas.bvh_nodes_offset = byte_offset / sizeof(BVHNode);

  bvh_nodes_size += blas.nodes_count;

  // Update map
  blas_allocations_map[blas_index] = {.tri_id_allocation = p_tri_ids,
                                      .bvh_nodes_allocation = p_bvh_nodes};

  // Stage ids data
  {
    std::span<u32> data_view = std::span(tri_ids_data, trig_count);
    staging_buffer.stage(data_view.data(), tri_ids_buffer,
                         tri_id_index * sizeof(u32), trig_count * sizeof(u32));
  }
  // Stage bvh data
  {
    staging_buffer.stage(p_bvh_nodes, bvh_nodes_buffer, byte_offset,
                         blas.nodes_count * sizeof(BVHNode));
  }
  {
    staging_buffer.stage(&blas, blas_buffer, sizeof(BLAS) * blas_index,
                         sizeof(BLAS));
  }

  return blas_index;
}

// TODO: Remove the transform parameter
u32 Renderer::add_blas_instance(u32 blas_index, const glm::mat4 &transform,
                                const MaterialHandle material) {
  // TODO: Check if material handle is valid
  u32 index = blas_inst_index_pool.obtain_new();
  BLASInstance &inst = blas_instances[index];
  inst.blas_id = blas_index;
  inst.set_transform(transform);
  inst.material_handle = material;
  staging_buffer.stage(&inst, blas_instances_buffer,
                       sizeof(BLASInstance) * index, sizeof(BLASInstance));
  rebuild_tlas = true;
  blas_instance_ids.insert(index);

  // Incrememnt material ref count
  switch (material.type) {
  case MaterialType::LAMBERT: {
    ++lambert_mats.reference_counts[material.index];
    break;
  }
  case MaterialType::METAL: {
    ++metal_mats.reference_counts[material.index];
    break;
  }
  case MaterialType::DIELECTRIC: {
    ++dielectric_mats.reference_counts[material.index];
    break;
  }
  case MaterialType::EMISSIVE: {
    ++emissive_mats.reference_counts[material.index];
    break;
  }
  default: {
    HCRITICAL("Unknown MaterialType");
    break;
  }
  }

  // Increment blas use
  blas_use_count[blas_index]++;
  return index;
}

void Renderer::set_blas_instance_transform(u32 blas_instance_id,
                                           const glm::mat4 &transform) {
  // Check if blas_instance exists
  if (!blas_instance_ids.contains(blas_instance_id)) {
    HWARN("Renderer::set_blas_instance_transform() - Trying to update an "
          "invalid blas_instance_id!.");
    return;
  }
  BLASInstance &inst = blas_instances[blas_instance_id];
  inst.set_transform(transform);
  staging_buffer.stage(&inst, blas_instances_buffer,
                       sizeof(BLASInstance) * blas_instance_id,
                       sizeof(BLASInstance));

  rebuild_tlas = true;
}

void Renderer::remove_blas(u32 blas_id) {
  if (!blas_allocations_map.contains(blas_id)) {
    HWARN("Renderer::remove_blas() - Trying to remove a blas_id with no "
          "allocation data!.");
    return;
  }

  --blas_use_count[blas_id];

  if (blas_use_count[blas_id] != 0) {
    return;
  }

  BLAS_Allocation &allocation = blas_allocations_map[blas_id];
  tri_id_allocator.deallocate(allocation.tri_id_allocation);
  bvh_nodes_allocator.deallocate(allocation.bvh_nodes_allocation);
  blases_index_pool.release(blas_id);
  blas_allocations_map.erase(blas_id);
}

void Renderer::remove_blas_instance(u32 blas_instance_id) {
  // TODO: Check if blas_instance_id is valid
  remove_material(blas_instances[blas_instance_id].material_handle);

  blas_inst_index_pool.release(blas_instance_id);
  blas_instance_ids.erase(blas_instance_id);
  remove_blas(blas_instances[blas_instance_id].blas_id);
  rebuild_tlas = true;
}

void Renderer::load_sphere_data() {
  HASSERT_MSG(sphere_blas_index == UINT32_MAX,
              "Renderer::load_sphere_data() should only be called once");
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> uvs;
  std::vector<u32> indices;
  generate_sphere(positions, indices, normals, uvs, 0.5f, 64, 32,
                  glm::vec3(0.f));

  sphere_blas_index = add_blas(positions, normals, uvs, indices);

  // Set it to 1 to ensure it can never be deleted by the user
  blas_use_count[sphere_blas_index] = 1;
}

void Renderer::load_cube_data() {
  HASSERT_MSG(cube_blas_index == UINT32_MAX,
              "Renderer::load_cube_data() should only be called once");
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> uvs;
  std::vector<u32> indices;
  generate_cube(positions, indices, normals, uvs, glm::vec3(0.f), 1.f, 1.f,
                1.f);
  cube_blas_index = add_blas(positions, normals, uvs, indices);

  // Set it to 1 to ensure it can never be deleted by the user
  blas_use_count[cube_blas_index] = 1;
}

void Renderer::load_plane_data() {
  HASSERT_MSG(plane_blas_index == UINT32_MAX,
              "Renderer::load_plane_data() should only be called once");
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> uvs;
  std::vector<u32> indices;
  generate_plane(positions, indices, normals, uvs, 1.f, 1.f, 1, 1,
                 glm::vec3(0.f));
  plane_blas_index = add_blas(positions, normals, uvs, indices);

  // Set it to 1 to ensure it can never be deleted by the user
  blas_use_count[plane_blas_index] = 1;
}

void Renderer::build_tlas() {
  tlas_nodes.resize(blas_inst_index_pool.size * 2);
  if (tlas_nodes.size()) {
    Clock clock;
    clock.start();
    std::vector<u32> temp_blas_instance_ids(blas_instance_ids.begin(),
                                            blas_instance_ids.end());
    tlas.build(tlas_nodes, blas_instances, temp_blas_instance_ids, blases,
               std::span<BVHNode>(
                   reinterpret_cast<BVHNode *>(bvh_nodes_allocator.memory),
                   bvh_nodes_allocator.max_size / sizeof(BVHNode)));
    HINFO("TLAS build time: {}s", clock.get_elapsed_time_s());

    staging_buffer.stage(tlas_nodes.data(), tlas_nodes_buffer, 0,
                         tlas.node_count * sizeof(TLASNode));
    staging_buffer.flush();
  }
  rebuild_tlas = false;
  frame_index = 0;
}

} // namespace hlx
