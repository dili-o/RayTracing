#include "Renderer.hpp"
#include "BVHNode.hpp"
#include "Core/Assert.hpp"
#include "Core/Clock.hpp"
#include "Core/Defines.hpp"
#include "Core/Exceptions.hpp"
#include "Material.hpp"
#include "Triangle.hpp"
#include "Vulkan/VkDeviceManager.h"
#include "Vulkan/VkResourceManager.hpp"
#include "Vulkan/VkShaderCompilation.h"
#include "Vulkan/VkStagingBuffer.h"
#include "Vulkan/VkUtils.hpp"
// Vendor
#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#define SAMPLES_PER_FRAME 1u

static constexpr VkFormat output_image_format = VK_FORMAT_R32G32B32A32_SFLOAT;

namespace hlx {
struct alignas(16) UniformData {
  glm::vec4 pixel00_loc;
  glm::vec4 pixel_delta_u;
  glm::vec4 pixel_delta_v;
  glm::vec4 camera_center;
  VkDeviceAddress triangle_geom_buffer;
  VkDeviceAddress triangle_shading_buffer;
  VkDeviceAddress bvh_nodes_buffer;
  VkDeviceAddress tri_ids_buffer;
  VkDeviceAddress triangle_mat_ids_buffer;
  VkDeviceAddress lambert_materials_buffer;
  VkDeviceAddress metal_materials_buffer;
  VkDeviceAddress dielectric_materials_buffer;
  VkDeviceAddress emissive_materials_buffer;
};

void generate_sphere(std::vector<glm::vec3> &out_vertices,
                     std::vector<uint32_t> &out_indices,
                     std::vector<glm::vec3> &out_normals, float radius,
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
                    std::vector<glm::vec3> &out_normals, float width,
                    float depth, uint32_t x_segments, uint32_t z_segments,
                    const glm::vec3 &center = glm::vec3(0.f)) {
  uint32_t vertex_offset = static_cast<uint32_t>(out_vertices.size());

  // Plane lies on XZ, normal pointing up (Y+)
  for (uint32_t z = 0; z <= z_segments; ++z) {
    float vz = float(z) / z_segments;
    float pos_z = (vz - 0.5f) * depth;

    for (uint32_t x = 0; x <= x_segments; ++x) {
      float ux = float(x) / x_segments;
      float pos_x = (ux - 0.5f) * width;

      glm::vec3 position = center + glm::vec3(pos_x, 0.f, pos_z);
      glm::vec3 normal = glm::vec3(0.f, 1.f, 0.f);

      out_vertices.push_back(position);
      out_normals.push_back(normal);
    }
  }

  // Indices
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

struct PushConstant {
  VkDeviceAddress uniform_data_buffer;
  u32 image_width;
  u32 image_height;
  u32 triangle_count;
  u32 frame_index;
};

void Renderer::init(VkDeviceManager *p_device, VkResourceManager *p_rm,
                    VkStagingBuffer &staging_buffer, u32 output_image_width,
                    u32 output_image_height) {
  HASSERT(p_device);
  HASSERT(p_rm);
  this->p_device = p_device;
  this->p_rm = p_rm;

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

  // Create path tracing shader
  ShaderHandle path_tracing_shader;
  try {
    ShaderBlob blob;
    SlangCompiler::compile_code("compute_main", "RayTracing",
                                SHADER_PATH "RayTracing.slang", blob);
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
  VkPipelineLayoutCreateInfo pipeline_layout_info{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &vk_set_layout;
  pipeline_layout_info.pushConstantRangeCount = 1;
  pipeline_layout_info.pPushConstantRanges = &push_constant;

  VkComputePipelineCreateInfo pipelien_create_info{
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  pipelien_create_info.stage = shader_stage_info;
  path_tracing_pipeline = p_rm->create_compute_pipeline(
      "PathTracingPipeline", pipelien_create_info, pipeline_layout_info);

  p_rm->queue_destroy({path_tracing_shader});

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

  // Create circles
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<u32> indices;
  std::vector<MaterialHandle> handles;
  std::vector<Lambert> lambert_mats;
  std::vector<Metal> metal_mats;
  std::vector<Emissive> emissive_mats;
  size_t index_offset = 0;
  // center
  lambert_mats.push_back({0.1f, 0.2f, 0.5f, 1.f});
  MaterialHandle center_mat = {0, MATERIAL_LAMBERT};
  generate_sphere(positions, indices, normals, 0.5f, 16, 8,
                  glm::vec3(0.f, 0.f, -1.2f));
  size_t trig_count = (indices.size() - index_offset) / 3;
  handles.reserve(trig_count);
  for (size_t i = 0; i < trig_count; ++i) {
    handles.push_back(center_mat);
  }
  index_offset = indices.size();
  // left
  metal_mats.push_back({0.8f, 0.8f, 0.8f, 0.3f});
  MaterialHandle left_mat = {0, MATERIAL_METAL};
  generate_sphere(positions, indices, normals, 0.5f, 16, 8,
                  glm::vec3(-1.f, 0.f, -1.f));
  trig_count = (indices.size() - index_offset) / 3;
  handles.reserve(trig_count);
  for (size_t i = 0; i < trig_count; ++i) {
    handles.push_back(left_mat);
  }
  index_offset = indices.size();
  // right
  emissive_mats.push_back({5.f, 5.f, 5.f, 1.f});
  MaterialHandle right_mat = {0, MATERIAL_EMISSIVE};
  generate_sphere(positions, indices, normals, 0.25f, 16, 8,
                  glm::vec3(-0.5f, 2.f, -1.f));
  trig_count = (indices.size() - index_offset) / 3;
  handles.reserve(trig_count);
  for (size_t i = 0; i < trig_count; ++i) {
    handles.push_back(right_mat);
  }
  index_offset = indices.size();
  // ground
  lambert_mats.push_back({0.8f, 0.8f, 0.0f, 1.0f});
  MaterialHandle ground_mat = {1, MATERIAL_LAMBERT};
  generate_plane(positions, indices, normals, 100.f, 100.f, 1, 1,
                 glm::vec3(0.f, -0.5f, 0.f));
  trig_count = (indices.size() - index_offset) / 3;
  handles.reserve(trig_count);
  for (size_t i = 0; i < trig_count; ++i) {
    handles.push_back(ground_mat);
  }

  std::vector<glm::vec3> triangle_centroids;
  std::vector<TriangleGeom> triangle_positions;
  std::vector<TriangleShading> triangle_surface_data;
  std::vector<u32> triangle_ids;
  for (size_t i = 0; i < indices.size(); i += 3) {
    triangle_positions.push_back(TriangleGeom(positions[indices[i]],
                                              positions[indices[i + 1]],
                                              positions[indices[i + 2]]));
    triangle_surface_data.push_back(TriangleShading(
        normals[indices[i]], normals[indices[i + 1]], normals[indices[i + 2]]));
    triangle_centroids.push_back((positions[indices[i]] +
                                  positions[indices[i + 1]] +
                                  positions[indices[i + 2]]) *
                                 0.3333f);
    triangle_ids.push_back(triangle_ids.size());
  }
  triangle_count = triangle_positions.size();

  // BVH
  u32 root_node_idx = 0, nodes_used = 1;
  std::vector<BVHNode> bvh_nodes(triangle_count * 2 - 1);
  BVHNode &root = bvh_nodes[root_node_idx];
  root.left_first = 0;
  root.tri_count = triangle_count;
  Clock clock;
  clock.start();
  update_node_bounds(bvh_nodes, triangle_positions, triangle_ids,
                     root_node_idx);
  subdivide(bvh_nodes, triangle_positions, triangle_centroids, triangle_ids,
            root_node_idx, nodes_used);
  HINFO("BVH build time: {}", clock.get_elapsed_time_s());

  // Create the buffers
  VmaAllocationCreateInfo vma_alloc_info{
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
  VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer_info.size = triangle_positions.size() * sizeof(TriangleGeom);
  buffer_info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  triangle_geom_buffer =
      p_rm->create_buffer("TriangleGeomBuffer", buffer_info, vma_alloc_info);

  buffer_info.size = triangle_positions.size() * sizeof(TriangleShading);
  triangle_shading_buffer =
      p_rm->create_buffer("TriangleShadingBuffer", buffer_info, vma_alloc_info);

  buffer_info.size = handles.size() * sizeof(MaterialHandle);
  triangle_mat_ids_buffer =
      p_rm->create_buffer("TriangleMatIdsBuffer", buffer_info, vma_alloc_info);

  buffer_info.size = lambert_mats.size() * sizeof(Lambert);
  lambert_materials_buffer = p_rm->create_buffer("LambertMaterialsBuffer",
                                                 buffer_info, vma_alloc_info);
  buffer_info.size = metal_mats.size() * sizeof(Metal);
  metal_materials_buffer =
      p_rm->create_buffer("MetalMaterialsBuffer", buffer_info, vma_alloc_info);

  // TODO: Dielectrics

  buffer_info.size = emissive_mats.size() * sizeof(Emissive);
  emissive_materials_buffer = p_rm->create_buffer("EmissiveMaterialsBuffer",
                                                  buffer_info, vma_alloc_info);
  buffer_info.size = nodes_used * sizeof(BVHNode);
  bvh_nodes_buffer =
      p_rm->create_buffer("BVHNodesBuffer", buffer_info, vma_alloc_info);
  buffer_info.size = triangle_ids.size() * sizeof(u32);
  tri_ids_buffer =
      p_rm->create_buffer("TriangleIDsBuffer", buffer_info, vma_alloc_info);

  VulkanBuffer *vk_trig_pos = p_rm->access_buffer(triangle_geom_buffer);
  VulkanBuffer *vk_trig_shad_data =
      p_rm->access_buffer(triangle_shading_buffer);
  VulkanBuffer *vk_trig_mats_buffer =
      p_rm->access_buffer(triangle_mat_ids_buffer);
  VulkanBuffer *vk_lamberts = p_rm->access_buffer(lambert_materials_buffer);
  VulkanBuffer *vk_metals = p_rm->access_buffer(metal_materials_buffer);
  VulkanBuffer *vk_emissives = p_rm->access_buffer(emissive_materials_buffer);
  VulkanBuffer *vk_bvh_nodes = p_rm->access_buffer(bvh_nodes_buffer);
  VulkanBuffer *vk_tri_ids = p_rm->access_buffer(tri_ids_buffer);

  staging_buffer.stage(triangle_positions.data(), triangle_geom_buffer, 0,
                       vk_trig_pos->vk_device_size);
  staging_buffer.stage(triangle_surface_data.data(), triangle_shading_buffer, 0,
                       vk_trig_shad_data->vk_device_size);
  staging_buffer.stage(handles.data(), triangle_mat_ids_buffer, 0,
                       vk_trig_mats_buffer->vk_device_size);
  staging_buffer.stage(lambert_mats.data(), lambert_materials_buffer, 0,
                       vk_lamberts->vk_device_size);
  staging_buffer.stage(metal_mats.data(), metal_materials_buffer, 0,
                       vk_metals->vk_device_size);
  staging_buffer.stage(emissive_mats.data(), emissive_materials_buffer, 0,
                       vk_emissives->vk_device_size);
  staging_buffer.stage(bvh_nodes.data(), bvh_nodes_buffer, 0,
                       vk_bvh_nodes->vk_device_size);
  staging_buffer.stage(triangle_ids.data(), tri_ids_buffer, 0,
                       vk_tri_ids->vk_device_size);
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
}

void Renderer::shutdown() {
  HASSERT(p_rm);

  for (BufferHandle &handle : uniform_buffers) {
    p_rm->queue_destroy({handle});
  }
  p_rm->queue_destroy({tri_ids_buffer});
  p_rm->queue_destroy({bvh_nodes_buffer});
  p_rm->queue_destroy({emissive_materials_buffer});
  p_rm->queue_destroy({metal_materials_buffer});
  p_rm->queue_destroy({lambert_materials_buffer});
  p_rm->queue_destroy({triangle_mat_ids_buffer});
  p_rm->queue_destroy({triangle_shading_buffer});
  p_rm->queue_destroy({triangle_geom_buffer});
  p_rm->queue_destroy({set_layout});
  p_rm->queue_destroy({output_image_view});
  p_rm->queue_destroy({path_tracing_pipeline});
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
  // Reset frame number if cam has moved
  if (camera.changed) {
    frame_index = 0;
    camera.changed = false;
  }
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
      .bvh_nodes_buffer =
          p_rm->access_buffer(bvh_nodes_buffer)->vk_device_address,
      .tri_ids_buffer = p_rm->access_buffer(tri_ids_buffer)->vk_device_address,
      .triangle_mat_ids_buffer =
          p_rm->access_buffer(triangle_mat_ids_buffer)->vk_device_address,
      .lambert_materials_buffer =
          p_rm->access_buffer(lambert_materials_buffer)->vk_device_address,
      .metal_materials_buffer =
          p_rm->access_buffer(metal_materials_buffer)->vk_device_address,
      .emissive_materials_buffer =
          p_rm->access_buffer(emissive_materials_buffer)->vk_device_address,
      // TODO:
      // VkDeviceAddress dielectric_materials_buffer;
  };
  VulkanBuffer *uniform_buffer =
      p_rm->access_buffer(uniform_buffers.at(p_device->current_frame));
  std::memcpy(uniform_buffer->p_data, &uniform_data, sizeof(UniformData));

  VkCommandBuffer cmd = p_device->get_current_cmd_buffer();

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
  push_constant.triangle_count = triangle_count;
  push_constant.uniform_data_buffer = uniform_buffer->vk_device_address;
  vkCmdPushConstants(cmd, pipeline->vk_pipeline_layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant),
                     &push_constant);
  const VkBindDescriptorSetsInfo bind_info{
      .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      .layout = pipeline->vk_pipeline_layout,
      .firstSet = 0,
      .descriptorSetCount = 1,
      .pDescriptorSets = &vk_set,
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

} // namespace hlx
