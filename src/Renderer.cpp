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
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/matrix.hpp"
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

struct alignas(16) SkyConstants {
  glm::mat4 gViewProjMat;

  glm::vec4 gColor;

  glm::vec3 gSunIlluminance;
  i32 gScatteringMaxPathDepth;

  glm::uvec2 gResolution;
  f32 gFrameTimeSec;
  f32 gTimeSec;

  glm::uvec2 gMouseLastDownPos;
  u32 gFrameId;
  u32 gTerrainResolution;

  glm::vec2 RayMarchMinMaxSPP;
  f32 gScreenshotCaptureActive;
  f32 pad;
};

struct alignas(16) SkyAtmosphereConstants {
  //
  // From AtmosphereParameters
  //

  glm::vec3 solar_irradiance;
  f32 sun_angular_radius;

  glm::vec3 absorption_extinction;
  f32 mu_s_min;

  glm::vec3 rayleigh_scattering;
  f32 mie_phase_function_g;

  glm::vec3 mie_scattering;
  f32 bottom_radius;

  glm::vec3 mie_extinction;
  f32 top_radius;

  glm::vec3 mie_absorption;
  f32 pad00;

  glm::vec3 ground_albedo;
  f32 pad0;

  glm::vec4 rayleigh_density[3];
  glm::vec4 mie_density[3];
  glm::vec4 absorption_density[3];

  //
  // Add generated static header constant
  //

  i32 TRANSMITTANCE_TEXTURE_WIDTH;
  i32 TRANSMITTANCE_TEXTURE_HEIGHT;
  i32 IRRADIANCE_TEXTURE_WIDTH;
  i32 IRRADIANCE_TEXTURE_HEIGHT;

  i32 SCATTERING_TEXTURE_R_SIZE;
  i32 SCATTERING_TEXTURE_MU_SIZE;
  i32 SCATTERING_TEXTURE_MU_S_SIZE;
  i32 SCATTERING_TEXTURE_NU_SIZE;

  glm::vec3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
  f32 pad3;

  glm::vec3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
  f32 pad4;

  //
  // Other globals
  //
  glm::mat4 gSkyViewProjMat;
  glm::mat4 gSkyInvViewProjMat;
  glm::mat4 gSkyInvProjMat;
  glm::mat4 gSkyInvViewMat;
  glm::mat4 gShadowmapViewProjMat;

  glm::vec3 camera;
  f32 pad5;

  glm::vec3 sun_direction;
  f32 pad6;

  glm::vec3 view_ray;
  f32 pad7;

  f32 MultipleScatteringFactor;
  f32 MultiScatteringLUTRes;
  f32 pad9;
  f32 pad10;
};

static constexpr u32 TRANSMITTANCE_TEXTURE_WIDTH = 256;
static constexpr u32 TRANSMITTANCE_TEXTURE_HEIGHT = 64;

struct LookUpTablesInfo {
  u32 TRANSMITTANCE_TEXTURE_WIDTH = 256;
  u32 TRANSMITTANCE_TEXTURE_HEIGHT = 64;

  u32 SCATTERING_TEXTURE_R_SIZE = 32;
  u32 SCATTERING_TEXTURE_MU_SIZE = 128;
  u32 SCATTERING_TEXTURE_MU_S_SIZE = 32;
  u32 SCATTERING_TEXTURE_NU_SIZE = 8;

  u32 IRRADIANCE_TEXTURE_WIDTH = 64;
  u32 IRRADIANCE_TEXTURE_HEIGHT = 16;

  // Derived from above
  u32 SCATTERING_TEXTURE_WIDTH = 0xDEADBEEF;
  u32 SCATTERING_TEXTURE_HEIGHT = 0xDEADBEEF;
  u32 SCATTERING_TEXTURE_DEPTH = 0xDEADBEEF;

  void updateDerivedData() {
    SCATTERING_TEXTURE_WIDTH =
        SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE;
    SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE;
    SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_R_SIZE;
  }

  LookUpTablesInfo() { updateDerivedData(); }
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
  timer.start();
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

  // Sky rendering init ///////////////////////////////////////////////
  // Constant buffers
  vma_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    buffer_info.size = sizeof(SkyConstants);
    sky_constant_buffers[i] = p_rm->create_buffer(
        "SkyConstantBuffer" + std::to_string(i), buffer_info, vma_alloc_info);
    buffer_info.size = sizeof(SkyAtmosphereConstants);
    sky_atmosphere_buffers[i] =
        p_rm->create_buffer("SkyAtmosphereConstantBuffer" + std::to_string(i),
                            buffer_info, vma_alloc_info);
  }

  // Transmittance LUT Texture
  VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = TRANSMITTANCE_TEXTURE_WIDTH;
  image_info.extent.height = TRANSMITTANCE_TEXTURE_HEIGHT;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  vma_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = image_info.format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = image_info.mipLevels;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  transmittance_lut_texture = p_rm->create_image_view(
      "TransmittanceLUTImageView", "TransmittanceLUTImage", image_info,
      vma_alloc_info, view_info);

  image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
  dummy_texture = p_rm->create_image_view(
      "DummyImageView", "DummyImage", image_info, vma_alloc_info, view_info);

  // Sky cb set layout
  VkDescriptorSetLayoutBinding sky_cb_bindings[2] = {

      VkDescriptorSetLayoutBinding{.binding = 0,
                                   .descriptorType =
                                       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                   .descriptorCount = 1,
                                   .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                                                 VK_SHADER_STAGE_FRAGMENT_BIT,
                                   .pImmutableSamplers = nullptr},
      VkDescriptorSetLayoutBinding{.binding = 1,
                                   .descriptorType =
                                       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                   .descriptorCount = 1,
                                   .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                   .pImmutableSamplers = nullptr}};

  layout_info.bindingCount = ArraySize(sky_cb_bindings);
  layout_info.pBindings = sky_cb_bindings;

  sky_cb_set_layout =
      p_rm->create_descriptor_set_layout("SkyCBSetLayout", layout_info);

  // Sky cb set
  std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> vk_set_layouts;
  vk_set_layouts.fill(p_rm->access_set_layout(sky_cb_set_layout)->vk_handle);

  VkDescriptorSetAllocateInfo set_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = p_device->vk_descriptor_pool,
      .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
      .pSetLayouts = vk_set_layouts.data()};
  VK_CHECK(vkAllocateDescriptorSets(p_device->vk_device, &set_alloc_info,
                                    sky_cb_sets.data()));

  // Write to sky cb sets
  VkDescriptorBufferInfo sky_constants_buffer_infos[MAX_FRAMES_IN_FLIGHT];
  VkDescriptorBufferInfo sky_atmosphere_buffer_infos[MAX_FRAMES_IN_FLIGHT];

  std::array<VkWriteDescriptorSet, MAX_FRAMES_IN_FLIGHT * 2> write_infos{};
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    // Sky constants buffers
    VkDescriptorBufferInfo &sky_constants_buffer_info =
        sky_constants_buffer_infos[i];
    sky_constants_buffer_info.buffer =
        p_rm->access_buffer(sky_constant_buffers[i])->vk_handle;
    sky_constants_buffer_info.offset = 0;
    sky_constants_buffer_info.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet &sky_constants_write_info = write_infos[i * 2];
    sky_constants_write_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sky_constants_write_info.dstSet = sky_cb_sets[i];
    sky_constants_write_info.dstBinding = 0;
    sky_constants_write_info.dstArrayElement = 0;
    sky_constants_write_info.descriptorCount = 1;
    sky_constants_write_info.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sky_constants_write_info.pBufferInfo = &sky_constants_buffer_info;
    // Sky atmosphere buffers
    VkDescriptorBufferInfo &sky_atmosphere_buffer_info =
        sky_atmosphere_buffer_infos[i];
    sky_atmosphere_buffer_info.buffer =
        p_rm->access_buffer(sky_atmosphere_buffers[i])->vk_handle;
    sky_atmosphere_buffer_info.offset = 0;
    sky_atmosphere_buffer_info.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet &sky_atmosphere_write_info = write_infos[(i * 2) + 1];
    sky_atmosphere_write_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sky_atmosphere_write_info.dstSet = sky_cb_sets[i];
    sky_atmosphere_write_info.dstBinding = 1;
    sky_atmosphere_write_info.dstArrayElement = 0;
    sky_atmosphere_write_info.descriptorCount = 1;
    sky_atmosphere_write_info.descriptorType =
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sky_atmosphere_write_info.pBufferInfo = &sky_atmosphere_buffer_infos[i];
  }
  vkUpdateDescriptorSets(p_device->vk_device, write_infos.size(),
                         write_infos.data(), 0, nullptr);

  // Sky texture set layout
  VkDescriptorSetLayoutBinding sky_texture_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr};

  layout_info.bindingCount = 1;
  layout_info.pBindings = &sky_texture_binding;

  sky_textures_set_layout =
      p_rm->create_descriptor_set_layout("SkyTextureSetLayout", layout_info);

  // Sky texture set
  set_alloc_info.descriptorSetCount = 1;
  set_alloc_info.pSetLayouts =
      &p_rm->access_set_layout(sky_textures_set_layout)->vk_handle;
  VK_CHECK(vkAllocateDescriptorSets(p_device->vk_device, &set_alloc_info,
                                    &sky_textures_set));

  // Write to sky cb sets
  VkDescriptorImageInfo sky_texture_image_info = {
      .sampler = p_rm->access_sampler(texture_sampler)->vk_handle,
      .imageView = p_rm->access_image_view(dummy_texture)->vk_handle,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet &sky_textures_write_info = write_infos[0];
  sky_textures_write_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  sky_textures_write_info.dstSet = sky_textures_set;
  sky_textures_write_info.dstBinding = 0;
  sky_textures_write_info.dstArrayElement = 0;
  sky_textures_write_info.descriptorCount = 1;
  sky_textures_write_info.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sky_textures_write_info.pBufferInfo = nullptr;
  sky_textures_write_info.pImageInfo = &sky_texture_image_info;
  vkUpdateDescriptorSets(p_device->vk_device, 1, write_infos.data(), 0,
                         nullptr);

  // Sky Rendering Pipeline
  VkPipelineRenderingCreateInfo rendering_info{
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
  rendering_info.colorAttachmentCount = 1;
  rendering_info.pColorAttachmentFormats = &image_info.format;
  rendering_info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
  rendering_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

  ShaderHandle transmittance_lut_vs_shader;
  ShaderHandle transmittance_lut_fs_shader;
  try {
    ShaderBlob blob;
    SlangCompiler::compile_code("ScreenTriangleVertexShader", "Common",
                                SHADER_PATH "SkyRendering/Common.slang", blob);
    transmittance_lut_vs_shader = p_rm->create_shader("", blob);

    SlangCompiler::compile_code(
        "RenderTransmittanceLutPS", "RenderSkyRayMarching",
        SHADER_PATH "SkyRendering/RenderSkyRayMarching.slang", blob);
    transmittance_lut_fs_shader = p_rm->create_shader("", blob);
  } catch (Exception exception) {
    HCRITICAL("{}", exception.what());
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = p_rm->access_shader(transmittance_lut_vs_shader)->vk_handle,
          .pName = "main",
          .pSpecializationInfo = nullptr},
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = p_rm->access_shader(transmittance_lut_fs_shader)->vk_handle,
          .pName = "main",
          .pSpecializationInfo = nullptr},
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vertex_input_info.vertexBindingDescriptionCount = 0;
  vertex_input_info.pVertexBindingDescriptions = nullptr;
  vertex_input_info.vertexAttributeDescriptionCount = 0;
  vertex_input_info.pVertexAttributeDescriptions = nullptr;

  VkPipelineInputAssemblyStateCreateInfo input_assmebly_info{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  input_assmebly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewport_info{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewport_info.viewportCount = 1;
  viewport_info.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo raster_state_info{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  raster_state_info.depthClampEnable = VK_FALSE;
  raster_state_info.rasterizerDiscardEnable = VK_FALSE;
  raster_state_info.polygonMode = VK_POLYGON_MODE_FILL;
  raster_state_info.lineWidth = 1.0f;
  raster_state_info.cullMode = VK_CULL_MODE_NONE;
  raster_state_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
  raster_state_info.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling_info{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisampling_info.sampleShadingEnable = VK_FALSE;
  multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_info{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  depth_stencil_info.depthTestEnable = VK_FALSE;
  depth_stencil_info.depthWriteEnable = VK_FALSE;
  depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
  depth_stencil_info.minDepthBounds = 0.0f;
  depth_stencil_info.maxDepthBounds = 1.0f;
  depth_stencil_info.stencilTestEnable = VK_FALSE;

  const VkPipelineColorBlendAttachmentState blend_attachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

  VkPipelineColorBlendStateCreateInfo color_blend_info{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  color_blend_info.logicOpEnable = VK_FALSE;
  color_blend_info.logicOp = VK_LOGIC_OP_CLEAR;
  color_blend_info.attachmentCount = 1;
  color_blend_info.pAttachments = &blend_attachment;

  std::array<VkDynamicState, 2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamic_info{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic_info.dynamicStateCount = dynamic_states.size();
  dynamic_info.pDynamicStates = dynamic_states.data();

  VkGraphicsPipelineCreateInfo pipeline_info{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pipeline_info.pNext = &rendering_info;
  pipeline_info.stageCount = shader_stages.size();
  pipeline_info.pStages = shader_stages.data();
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assmebly_info;
  pipeline_info.pViewportState = &viewport_info;
  pipeline_info.pRasterizationState = &raster_state_info;
  pipeline_info.pMultisampleState = &multisampling_info;
  pipeline_info.pDepthStencilState = &depth_stencil_info;
  pipeline_info.pColorBlendState = &color_blend_info;
  pipeline_info.pDynamicState = &dynamic_info;

  set_layouts[0] = p_rm->access_set_layout(sky_cb_set_layout)->vk_handle;
  set_layouts[1] = p_rm->access_set_layout(sky_textures_set_layout)->vk_handle;

  pipeline_layout_info.setLayoutCount = 2;
  pipeline_layout_info.pSetLayouts = set_layouts;
  pipeline_layout_info.pPushConstantRanges = nullptr;
  pipeline_layout_info.pushConstantRangeCount = 0;

  transmittance_lut_pipeline = p_rm->create_graphics_pipeline(
      "TransmittanceLUTPipeline", pipeline_info, pipeline_layout_info);

  p_rm->queue_destroy({transmittance_lut_vs_shader});
  p_rm->queue_destroy({transmittance_lut_fs_shader});
}

void Renderer::shutdown() {
  HASSERT(p_rm);

  // Sky rendering shutdown
  p_rm->queue_destroy({transmittance_lut_pipeline});
  p_rm->queue_destroy({sky_textures_set_layout});
  p_rm->queue_destroy({sky_cb_set_layout});

  p_rm->queue_destroy({dummy_texture});
  p_rm->queue_destroy({transmittance_lut_texture});

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    p_rm->queue_destroy({sky_constant_buffers[i]});
    p_rm->queue_destroy({sky_atmosphere_buffers[i]});
  }

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
  // Sky update
  static bool first_time = true;
  if (first_time) {
    // Transition dummy
    VulkanImageView *dummy_view = p_rm->access_image_view(dummy_texture);
    VulkanImage *dummy_image = p_rm->access_image(dummy_view->image_handle);
    VkImageMemoryBarrier2 image_barrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier.subresourceRange.baseMipLevel = 0;
    image_barrier.subresourceRange.levelCount = 1;
    image_barrier.subresourceRange.baseArrayLayer = 0;
    image_barrier.subresourceRange.layerCount = 1;
    image_barrier.image = dummy_image->vk_handle;
    image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    image_barrier.srcAccessMask = VK_ACCESS_2_NONE;
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    image_barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency_info.dependencyFlags = 0;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &image_barrier;
    vkCmdPipelineBarrier2(cmd, &dependency_info);
    first_time = false;
  }
  VulkanBuffer *sky_constants_buffer =
      p_rm->access_buffer(sky_constant_buffers.at(p_device->current_frame));
  VulkanBuffer *sky_atmosphere_buffer =
      p_rm->access_buffer(sky_atmosphere_buffers.at(p_device->current_frame));
  {
    SkyAtmosphereConstants cb;
    memset(&cb, 0xBA, sizeof(SkyAtmosphereConstants));
    cb.solar_irradiance = glm::vec3(1.f);
    cb.sun_angular_radius = 0.00467499997f;
    cb.absorption_extinction =
        glm::vec3(0.000650000002f, 0.00188100000f, 8.49999997e-05f);
    cb.mu_s_min = -0.500000060f;

    cb.rayleigh_density[0] = glm::vec4(0.f);
    cb.rayleigh_density[1] = glm::vec4(0.f, 0.f, 1.f, -0.125000000f);
    cb.rayleigh_density[2] =
        glm::vec4(0.f, 0.f, -0.00142463227f, -0.00142463227f);

    cb.mie_density[0] = glm::vec4(0.f);
    cb.mie_density[1] = glm::vec4(0.f, 0.f, 1.f, -0.833333313f);
    cb.mie_density[2] = glm::vec4(0.f, 0.f, -0.00142463227f, -0.00142463227f);

    cb.absorption_density[0] = glm::vec4(25.f, 0.f, 0.f, 0.0666666701f);
    cb.absorption_density[1] = glm::vec4(-0.666666687f, 0.f, 0.f, 0.f);
    cb.absorption_density[2] = glm::vec4(-0.0666666701f, 2.66666675f,
                                         -0.00142463227f, -0.00142463227f);

    cb.mie_phase_function_g = 0.800000012f;
    cb.rayleigh_scattering =
        glm::vec3(0.00580199994f, 0.0135580003f, 0.0331000015f);
    const float RayleighScatScale = 1.f;
    cb.rayleigh_scattering.x *= RayleighScatScale;
    cb.rayleigh_scattering.y *= RayleighScatScale;
    cb.rayleigh_scattering.z *= RayleighScatScale;
    cb.mie_scattering =
        glm::vec3(0.00399600016f, 0.00399600016f, 0.00399600016f);
    cb.mie_extinction =
        glm::vec3(0.00443999982f, 0.00443999982f, 0.00443999982f);
    cb.mie_absorption =
        glm::max(glm::vec3(0.f), cb.mie_extinction - cb.mie_scattering);
    cb.ground_albedo = glm::vec3(0.f);
    cb.bottom_radius = 6360.f;
    cb.top_radius = 6460.f;
    cb.MultipleScatteringFactor = 1;
    cb.MultiScatteringLUTRes = 32;

    //
    cb.TRANSMITTANCE_TEXTURE_WIDTH = TRANSMITTANCE_TEXTURE_WIDTH;
    cb.TRANSMITTANCE_TEXTURE_HEIGHT = TRANSMITTANCE_TEXTURE_HEIGHT;
    // cb.IRRADIANCE_TEXTURE_WIDTH = LutsInfo.IRRADIANCE_TEXTURE_WIDTH;
    // cb.IRRADIANCE_TEXTURE_HEIGHT = LutsInfo.IRRADIANCE_TEXTURE_HEIGHT;
    // cb.SCATTERING_TEXTURE_R_SIZE = LutsInfo.SCATTERING_TEXTURE_R_SIZE;
    // cb.SCATTERING_TEXTURE_MU_SIZE = LutsInfo.SCATTERING_TEXTURE_MU_SIZE;
    // cb.SCATTERING_TEXTURE_MU_S_SIZE = LutsInfo.SCATTERING_TEXTURE_MU_S_SIZE;
    // cb.SCATTERING_TEXTURE_NU_SIZE = LutsInfo.SCATTERING_TEXTURE_NU_SIZE;
    cb.SKY_SPECTRAL_RADIANCE_TO_LUMINANCE =
        glm::vec3(114974.916437f, 71305.954816f,
                  65310.548555f); // Not used if using LUTs as transfert
    cb.SUN_SPECTRAL_RADIANCE_TO_LUMINANCE =
        glm::vec3(98242.786222f, 69954.398112f, 66475.012354f); // idem

    //
    glm::mat4 projection =
        glm::perspective(glm::radians(camera.fov), 16.f / 9.f, 0.001f, 1000.f);
    projection[1][1] *= -1.f;
    glm::mat4 view = camera.get_view();

    cb.gSkyViewProjMat = projection * view;
    cb.gSkyInvViewProjMat = glm::inverse(cb.gSkyViewProjMat);
    cb.gSkyInvProjMat = glm::inverse(projection);
    cb.gSkyInvViewMat = glm::inverse(view);

    // cb.gShadowmapViewProjMat = mShadowmapViewProjMat;

    cb.camera = camera.position;
    cb.view_ray = glm::normalize(camera.look_at);
    cb.sun_direction = glm::vec3(0.00000000f, 0.900447130f, 0.434965521f);

    memcpy(sky_atmosphere_buffer->p_data, &cb, sizeof(SkyAtmosphereConstants));

    static f64 last_time_sec = timer.get_elapsed_time_s();
    static f64 elapsed_time_sec = 0.0;
    static f64 cur_time_sec = timer.get_elapsed_time_s();
    SkyConstants sky_cb;
    sky_cb.gViewProjMat = cb.gSkyViewProjMat;
    sky_cb.gColor = glm::vec4(0.f, 1.f, 1.f, 1.f);
    sky_cb.gSunIlluminance = glm::vec3(1.f);
    sky_cb.gScatteringMaxPathDepth = 4;
    sky_cb.gResolution = {1280, 720};
    sky_cb.gFrameTimeSec = cur_time_sec - last_time_sec;
    sky_cb.gTimeSec = elapsed_time_sec;

    sky_cb.gMouseLastDownPos = {0, 0};
    sky_cb.gFrameId = frame_index;
    sky_cb.gTerrainResolution = 512;

    sky_cb.RayMarchMinMaxSPP = {4, 14};
    sky_cb.gScreenshotCaptureActive = 0;
    elapsed_time_sec += sky_cb.gFrameTimeSec;
    memcpy(sky_constants_buffer->p_data, &sky_cb, sizeof(SkyConstants));
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

  // Sky rendering pass ////////
  push_debug_label(cmd, "TransLUT");
  // Transition render target
  VulkanImageView *transmittance_lut_view =
      p_rm->access_image_view(transmittance_lut_texture);
  VulkanImage *transmittance_lut_image =
      p_rm->access_image(transmittance_lut_view->image_handle);

  VkImageMemoryBarrier2 image_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_barrier.subresourceRange.baseMipLevel = 0;
  image_barrier.subresourceRange.levelCount = 1;
  image_barrier.subresourceRange.baseArrayLayer = 0;
  image_barrier.subresourceRange.layerCount = 1;
  image_barrier.image = transmittance_lut_image->vk_handle;
  image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  image_barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  image_barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  image_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency_info.dependencyFlags = 0;
  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers = &image_barrier;
  vkCmdPipelineBarrier2(cmd, &dependency_info);

  VkRenderingAttachmentInfo color_attachment_info{
      VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  color_attachment_info.imageView = transmittance_lut_view->vk_handle;
  color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
  VkRenderingInfo render_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
  render_info.layerCount = 1;
  render_info.renderArea = {{0, 0}, transmittance_lut_image->vk_extent};
  render_info.viewMask = 0;
  render_info.colorAttachmentCount = 1;
  render_info.pColorAttachments = &color_attachment_info;
  render_info.pDepthAttachment = nullptr;
  render_info.pStencilAttachment = nullptr;
  vkCmdBeginRendering(cmd, &render_info);

  VkViewport viewport = {.x = 0.f,
                         .y = 0.f,
                         .width = TRANSMITTANCE_TEXTURE_WIDTH,
                         .height = TRANSMITTANCE_TEXTURE_HEIGHT,
                         .minDepth = 0.f,
                         .maxDepth = 1.f};
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  VkRect2D scissor = {.offset = {0, 0},
                      .extent = transmittance_lut_image->vk_extent};
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  VulkanPipeline *trans_lut_pipeline =
      p_rm->access_pipeline(transmittance_lut_pipeline);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    trans_lut_pipeline->vk_handle);

  VkDescriptorSet sky_sets[] = {sky_cb_sets.at(p_device->current_frame),
                                sky_textures_set};
  const VkBindDescriptorSetsInfo sky_bind_info{
      .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .layout = trans_lut_pipeline->vk_pipeline_layout,
      .firstSet = 0,
      .descriptorSetCount = 2,
      .pDescriptorSets = sky_sets,
  };
  vkCmdBindDescriptorSets2(cmd, &sky_bind_info);

  vkCmdDraw(cmd, 3, 1, 0, 0);

  vkCmdEndRendering(cmd);
  pop_debug_label(cmd);
  //

  push_debug_label(cmd, "Compute Pass");

  // Transition output image to compute write bit
  image_barrier.image = vk_output_image->vk_handle;
  image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  image_barrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  image_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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
