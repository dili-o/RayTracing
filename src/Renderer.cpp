#include "Renderer.hpp"
#include "Core/Exceptions.hpp"
#include "Core/Log.hpp"
#include "Core/ResourcePool.hpp"
#include "Platform/Platform.hpp"
#include "Vulkan/VkShaderCompilation.h"
// Vendor

#define SAMPLES_PER_FRAME 1u

using namespace hlx;
static constexpr VkFormat image_format = VK_FORMAT_R32G32B32A32_SFLOAT;

struct UniformBuffer {
  VkDeviceAddress triangles;
  VkDeviceAddress tri_ids;
  VkDeviceAddress tlas_nodes;
  VkDeviceAddress bvhs;
  VkDeviceAddress bvh_nodes;
  VkDeviceAddress lambert_materials;
  VkDeviceAddress metal_materials;
  VkDeviceAddress dielectric_materials;
};

struct alignas(16) ShaderPushConstant {
  f32 pixel00_loc[4];
  f32 pixel_delta_u[4];
  f32 pixel_delta_v[4];
  f32 camera_center[4];

  u32 image_width;
  u32 image_height;
  u32 seed_value;
  u32 samples_per_pixel;

  f32 pixel_samples_scale;
  u32 max_depth;
  u32 triangle_count;
  f32 defocus_angle;

  f32 defocus_disk_u[4];
  f32 defocus_disk_v[4]; // 128
};

void Renderer::initialize_camera(u32 image_width_, real aspect_ratio_,
                                 u32 samples_per_pixel_, u32 max_depth_,
                                 real vfov_deg_) {
  aspect_ratio = aspect_ratio_;
  samples_per_pixel = samples_per_pixel_;
  max_depth = max_depth_;
  vfov = vfov_deg_;
  Platform::get_window_size(&image_width, &image_height);

  pixel_samples_scale = 1.f / samples_per_pixel;

  // Determine viewport dimensions.
  real theta = degrees_to_radians(vfov);
  real h = std::tan(theta / 2.f);
  real viewport_height = 2.f * h * focus_dist;
  real viewport_width = viewport_height * (real(image_width) / image_height);

  // Calculate the u,v,w unit basis vectors for the camera coordinate frame.
  w = unit_vector(center - lookat);
  u = unit_vector(cross(vup, w));
  v = cross(w, u);

  // Calculate the vectors across the horizontal and down the vertical
  // viewport edges.
  Vec3 viewport_u =
      viewport_width * u; // Vector across viewport horizontal edge
  Vec3 viewport_v = viewport_height * -v; // Vector down viewport vertical edge

  // Calculate the horizontal and vertical delta vectors from pixel to pixel.
  pixel_delta_u = viewport_u / (real)image_width;
  pixel_delta_v = viewport_v / (real)image_height;

  // Calculate the location of the upper left pixel.
  Vec3 viewport_upper_left =
      center - (focus_dist * w) - viewport_u / 2.f - viewport_v / 2.f;
  pixel00_loc = viewport_upper_left + 0.5f * (pixel_delta_u + pixel_delta_v);

  // Calculate the camera defocus disk basis vectors.
  real defocus_radius =
      focus_dist * std::tan(degrees_to_radians(defocus_angle / 2.f));
  defocus_disk_u = u * defocus_radius;
  defocus_disk_v = v * defocus_radius;
}

MaterialHandle Renderer::add_lambert_material(const Vec3 &albedo) {
  VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = 1;
  image_info.extent.height = 1;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1; // TODO: Mip generation
  image_info.arrayLayers = 1;
  image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

  ImageHandle image = p_resource_manager->create_image(
      "TODO_NAME_IMAGE", image_info, vma_alloc_info);
  ImageViewHandle image_view = p_resource_manager->create_image_view(
      "TODO_NAME_IMAGE_VIEW", image, view_info);
  vk_image_views.push_back(image_view);

  u8 *pixels = static_cast<u8 *>(malloc(sizeof(u8) * BYTES_PER_PIXEL));
  HASSERT(pixels);
  pixels[0] = static_cast<u8>(albedo.x * 255.f);
  pixels[1] = static_cast<u8>(albedo.y * 255.f);
  pixels[2] = static_cast<u8>(albedo.z * 255.f);
  pixels[3] = 255;

  staging_buffer.stage(pixels, image_view, sizeof(u8) * BYTES_PER_PIXEL);

  // images.emplace_back(pixels, 1, 1);
  lambert_mats.push_back({static_cast<u32>(vk_image_views.size() - 1)});
  return {MATERIAL_LAMBERT, ((u32)lambert_mats.size() - 1)};
}

MaterialHandle Renderer::add_lambert_material(const std::string &filename) {
  Image image_data = Image(filename.c_str(), true);

  VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = image_data.width();
  image_info.extent.height = image_data.height();
  image_info.extent.depth = 1;
  image_info.mipLevels = 1; // TODO: Mip generation
  image_info.arrayLayers = 1;
  image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

  ImageHandle image = p_resource_manager->create_image(
      "TODO_NAME_IMAGE", image_info, vma_alloc_info);
  ImageViewHandle image_view = p_resource_manager->create_image_view(
      "TODO_NAME_IMAGE_VIEW", image, view_info);
  vk_image_views.push_back(image_view);

  staging_buffer.stage(image_data.pixel_data(0, 0), image_view,
                       image_data.width() * image_data.height() *
                           BYTES_PER_PIXEL);

  // images.emplace_back(filename.c_str(), true);
  lambert_mats.push_back({static_cast<u32>(vk_image_views.size() - 1)});
  return {MATERIAL_LAMBERT, ((u32)lambert_mats.size() - 1)};
}

MaterialHandle Renderer::add_metal_material(const Vec3 &albedo,
                                            real fuzziness) {
  metal_mats.push_back({albedo.x, albedo.y, albedo.z, fuzziness});
  return {MATERIAL_METAL, ((u32)metal_mats.size() - 1)};
}

MaterialHandle Renderer::add_dielectric_material(real refraction_index) {
  dielectric_mats.push_back({refraction_index});
  return {MATERIAL_DIELECTRIC, ((u32)dielectric_mats.size() - 1)};
}

void Renderer::add_triangle(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2,
                            const Vec3 &n0, const Vec3 &n1, const Vec3 &n2,
                            Vec2 uv_0, Vec2 uv_1, Vec2 uv_2,
                            MaterialHandle mat_handle) {
  const Vec3 centroid = (v0 + v1 + v2) * 0.3333f;

  tri_centroids.push_back(centroid);
  tri_ids.push_back(static_cast<u32>(tri_ids.size()));
  switch (mat_handle.type) {
  case MATERIAL_LAMBERT: {
    HASSERT(mat_handle.index < lambert_mats.size());
    break;
  }
  case MATERIAL_METAL: {
    HASSERT(mat_handle.index < metal_mats.size());
    break;
  }
  case MATERIAL_DIELECTRIC: {
    HASSERT(mat_handle.index < dielectric_mats.size());
    break;
  }
  default: {
    HASSERT_MSG(false, "Invalid material type given");
  }
  }

  if (n0.x == std::numeric_limits<f32>::max()) {
    const Vec3 edge1 = v1 - v0;
    const Vec3 edge2 = v2 - v0;
    const Vec3 n = cross(edge1, edge2);
    triangles.push_back(
        TriangleGPU(v0, v1, v2, n, n, n, uv_0, uv_1, uv_2, mat_handle));
  } else {
    triangles.push_back(
        TriangleGPU(v0, v1, v2, n0, n1, n2, uv_0, uv_1, uv_2, mat_handle));
  }
}

u32 Renderer::get_triangle_count() { return triangles.size(); }

void Renderer::add_mesh(u32 triangles_offset, u32 triangle_count,
                        const Mat4 &transform) {
  u32 depth;
  bvhs.emplace_back(BVH(triangles.data(), triangle_count, triangles_offset,
                        true, tri_ids.data(), tri_centroids.data(), depth));
  BVH &bvh = bvhs[bvhs.size() - 1];
  bvh.set_transform(transform);

  BVH_GPU bvh_gpu;
  bvh_gpu.transform = bvh.inv_transform.inverse().transpose();
  bvh_gpu.inv_transform = bvh.inv_transform.transpose();
  bvh_gpu.node_index = bvh_nodes_size;
  bvh_gpu.trig_offset = triangles_offset;
  bvhs_gpu.push_back(bvh_gpu);

  bvh_nodes_size += bvh.bvh_nodes.size();
}

// TODO: Pass in p_resource_manager and p_device
void Renderer::init(u32 image_width_, real aspect_ratio_,
                    u32 samples_per_pixel_, u32 max_depth_, real vfov_deg_) {
  initialize_camera(image_width_, aspect_ratio_, samples_per_pixel_, max_depth_,
                    vfov_deg_);
  // Create Final image and view
  VkImageCreateInfo image_create_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image_create_info.imageType = VK_IMAGE_TYPE_2D;
  image_create_info.format = image_format;
  image_create_info.extent = {static_cast<u32>(image_width),
                              static_cast<u32>(image_height), 1};
  image_create_info.mipLevels = 1;
  image_create_info.arrayLayers = 1;
  image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_create_info.usage = VK_IMAGE_USAGE_STORAGE_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT |
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo vma_alloc_info{};
  vma_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  ImageHandle final_image = p_resource_manager->create_image(
      "FinalImage", image_create_info, vma_alloc_info);

  VkImageViewCreateInfo image_view_create_info{
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_create_info.format = image_format;
  image_view_create_info.subresourceRange.aspectMask =
      VK_IMAGE_ASPECT_COLOR_BIT;
  image_view_create_info.subresourceRange.baseMipLevel = 0;
  image_view_create_info.subresourceRange.levelCount = 1;
  image_view_create_info.subresourceRange.baseArrayLayer = 0;
  image_view_create_info.subresourceRange.layerCount = 1;

  final_image_view = p_resource_manager->create_image_view(
      "FinalImageView", final_image, image_view_create_info);

  VkDescriptorPoolSize scene_data_pools[2]{
      VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                           .descriptorCount = MAX_FRAMES_IN_FLIGHT},
      VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                           .descriptorCount = 1}};

  VkDescriptorPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1,
      .poolSizeCount = ArraySize(scene_data_pools),
      .pPoolSizes = scene_data_pools};

  VK_CHECK(vkCreateDescriptorPool(p_device->vk_device, &pool_info, nullptr,
                                  &vk_descriptor_pool));

  VkDescriptorPoolSize bindless_pool{
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = static_cast<u32>(vk_image_views.size()) +
                         1}; // Adding one for the final image
  pool_info.pPoolSizes = &bindless_pool;
  pool_info.poolSizeCount = 1;
  VK_CHECK(vkCreateDescriptorPool(p_device->vk_device, &pool_info, nullptr,
                                  &vk_bindless_descriptor_pool));

  // Pipeline
  ShaderBlob blob;
  try {
    VkCompileOptions opts;
    opts.add("ALBEDO_TEXTURE_COUNT", vk_image_views.size());
    SlangCompiler::compile_code("computeMain", "RayTracing",
                                SHADER_PATH "RayTracing.slang", blob, opts);
  } catch (Exception exception) {
    HERROR("{}", exception.what());
  }

  comp_shader_module =
      p_resource_manager->create_shader("RayTracingComp", blob);

  VkPipelineShaderStageCreateInfo shader_stage_info{
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shader_stage_info.module =
      p_resource_manager->access_shader(comp_shader_module)->vk_handle;
  shader_stage_info.pName = "main";

  // Triangles buffer
  VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  buffer_info.size = sizeof(TriangleGPU) * triangles.size();
  triangles_buffer = p_resource_manager->create_buffer(
      "TrianglesBuffer", buffer_info, vma_alloc_info);
  staging_buffer.stage(triangles.data(), triangles_buffer, 0, buffer_info.size);

  // Triangle IDs buffer
  buffer_info.size = sizeof(u32) * tri_ids.size();
  tri_ids_buffer = p_resource_manager->create_buffer(
      "TrianglesIDsBuffer", buffer_info, vma_alloc_info);
  staging_buffer.stage(tri_ids.data(), tri_ids_buffer, 0, buffer_info.size);

  // Lambert materials buffer
  buffer_info.size = sizeof(GpuLambert) * lambert_mats.size();
  if (buffer_info.size) {
    lambert_buffer = p_resource_manager->create_buffer(
        "LambertBuffer", buffer_info, vma_alloc_info);
    staging_buffer.stage(lambert_mats.data(), lambert_buffer, 0,
                         buffer_info.size);
  }

  // Metal materials buffer
  buffer_info.size = sizeof(GpuMetal) * metal_mats.size();
  if (buffer_info.size) {
    metal_buffer = p_resource_manager->create_buffer("MetalBuffer", buffer_info,
                                                     vma_alloc_info);
    staging_buffer.stage(metal_mats.data(), metal_buffer, 0, buffer_info.size);
  }

  // Dielectric materials buffer
  buffer_info.size = sizeof(GpuDielectric) * dielectric_mats.size();
  if (buffer_info.size) {
    dielectric_buffer = p_resource_manager->create_buffer(
        "DielectricBuffer", buffer_info, vma_alloc_info);
    staging_buffer.stage(dielectric_mats.data(), dielectric_buffer, 0,
                         buffer_info.size);
  }

  tlas = TLAS(bvhs.data(), bvhs.size());
  tlas.build();

  buffer_info.size = sizeof(TLASNode) * tlas.tlas_nodes.size();
  tlas_nodes_buffer = p_resource_manager->create_buffer(
      "TLASNodesBuffer", buffer_info, vma_alloc_info);
  staging_buffer.stage(tlas.tlas_nodes.data(), tlas_nodes_buffer, 0,
                       buffer_info.size);

  buffer_info.size = sizeof(BVH_GPU) * bvhs_gpu.size();
  bvhs_buffer = p_resource_manager->create_buffer("BVHsBuffer", buffer_info,
                                                  vma_alloc_info);
  staging_buffer.stage(bvhs_gpu.data(), bvhs_buffer, 0, buffer_info.size);

  buffer_info.size = sizeof(BVHNode) * bvh_nodes_size;
  bvh_nodes_buffer = p_resource_manager->create_buffer(
      "BVHsBuffer", buffer_info, vma_alloc_info);

  UniformBuffer uniform_buffer_data{};
  uniform_buffer_data.triangles =
      p_resource_manager->access_buffer(triangles_buffer)->vk_device_address;
  uniform_buffer_data.tri_ids =
      p_resource_manager->access_buffer(tri_ids_buffer)->vk_device_address;
  uniform_buffer_data.tlas_nodes =
      p_resource_manager->access_buffer(tlas_nodes_buffer)->vk_device_address;
  uniform_buffer_data.bvhs =
      p_resource_manager->access_buffer(bvhs_buffer)->vk_device_address;
  uniform_buffer_data.bvh_nodes =
      p_resource_manager->access_buffer(bvh_nodes_buffer)->vk_device_address;

  uniform_buffer_data.lambert_materials =
      is_handle_valid(lambert_buffer)
          ? p_resource_manager->access_buffer(lambert_buffer)->vk_device_address
          : 0;
  uniform_buffer_data.metal_materials =
      is_handle_valid(metal_buffer)
          ? p_resource_manager->access_buffer(metal_buffer)->vk_device_address
          : 0;
  uniform_buffer_data.dielectric_materials =
      is_handle_valid(dielectric_buffer)
          ? p_resource_manager->access_buffer(dielectric_buffer)
                ->vk_device_address
          : 0;

  // Uniform buffer
  buffer_info.usage =
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  buffer_info.size = sizeof(UniformBuffer);
  uniform_buffer = p_resource_manager->create_buffer(
      "UniformBuffer", buffer_info, vma_alloc_info);
  // Transfer data to Uniform buffer
  staging_buffer.stage(&uniform_buffer_data, uniform_buffer, 0,
                       buffer_info.size);

  // Sampler
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
  vk_sampler = p_resource_manager->create_sampler("Sampler", sampler_info);

  // Descriptor Set layout
  VkDescriptorSetLayoutBinding out_image_binding{};
  out_image_binding.binding = 0;
  out_image_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  out_image_binding.descriptorCount = 1;
  out_image_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding uniform_buffer_binding{};
  uniform_buffer_binding.binding = 1;
  uniform_buffer_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  uniform_buffer_binding.descriptorCount = 1;
  uniform_buffer_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding bindings[2] = {out_image_binding,
                                              uniform_buffer_binding};
  VkDescriptorSetLayoutCreateInfo layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layout_info.bindingCount = ArraySize(bindings);
  layout_info.pBindings = bindings;
  {
    vk_scene_data_set_layout = p_resource_manager->create_descriptor_set_layout(
        "SetLayout", layout_info);

    VkDescriptorSetLayout scene_layout =
        p_resource_manager->access_set_layout(vk_scene_data_set_layout)
            ->vk_handle;

    const VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vk_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &scene_layout};

    VK_CHECK(vkAllocateDescriptorSets(p_device->vk_device, &alloc_info,
                                      &vk_scene_data_set));

    VulkanBuffer *vk_uniform_buffer =
        p_resource_manager->access_buffer(uniform_buffer);
    const VkDescriptorBufferInfo buffer_info{
        .buffer = vk_uniform_buffer->vk_handle,
        .offset = 0,
        .range = vk_uniform_buffer->vk_device_size,
    };

    const VkDescriptorImageInfo image_info{
        .sampler = VK_NULL_HANDLE,
        .imageView =
            p_resource_manager->access_image_view(final_image_view)->vk_handle,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkWriteDescriptorSet write_infos[2] = {
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                             .dstSet = vk_scene_data_set,
                             .dstBinding = 0,
                             .dstArrayElement = 0,
                             .descriptorCount = 1,
                             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                             .pImageInfo = &image_info},
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                             .dstSet = vk_scene_data_set,
                             .dstBinding = 1,
                             .dstArrayElement = 0,
                             .descriptorCount = 1,
                             .descriptorType =
                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             .pBufferInfo = &buffer_info}};

    vkUpdateDescriptorSets(p_device->vk_device, 2, write_infos, 0, nullptr);
  }

  // Create bindless layout
  VkDescriptorSetLayoutBinding texture_array{};
  texture_array.binding = 0;
  texture_array.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  texture_array.descriptorCount = vk_image_views.size();
  texture_array.stageFlags =
      VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  bindings[0] = texture_array;
  layout_info.bindingCount = 1;

  vk_bindless_texture_set_layout =
      p_resource_manager->create_descriptor_set_layout("BindlessSetLayout",
                                                       layout_info);

  const VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = vk_bindless_descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts =
          &p_resource_manager->access_set_layout(vk_bindless_texture_set_layout)
               ->vk_handle};
  VK_CHECK(vkAllocateDescriptorSets(p_device->vk_device, &alloc_info,
                                    &vk_bindless_texture_set));

  // Pipeline layout
  VkDescriptorSetLayout set_layouts[2] = {
      p_resource_manager->access_set_layout(vk_scene_data_set_layout)
          ->vk_handle,
      p_resource_manager->access_set_layout(vk_bindless_texture_set_layout)
          ->vk_handle};

  VkPushConstantRange push_range{};
  push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  push_range.offset = 0;
  push_range.size = sizeof(ShaderPushConstant);

  VkPipelineLayoutCreateInfo pipeline_layout_info{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipeline_layout_info.setLayoutCount = ArraySize(set_layouts);
  pipeline_layout_info.pSetLayouts = set_layouts;
  pipeline_layout_info.pushConstantRangeCount = 1;
  pipeline_layout_info.pPushConstantRanges = &push_range;

  // Pipeline
  VkComputePipelineCreateInfo pipelien_create_info{
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  pipelien_create_info.stage = shader_stage_info;
  path_tracing_pipeline_handle = p_resource_manager->create_compute_pipeline(
      "PathTracerPipeline", pipelien_create_info, pipeline_layout_info);

  // Transfer data to buffers
  u32 bvh_nodes_offset = 0;
  for (size_t i = 0; i < bvhs.size(); ++i) {
    u32 copy_size = sizeof(BVHNode) * bvhs[i].bvh_nodes.size();
    staging_buffer.stage(bvhs[i].bvh_nodes.data(), bvh_nodes_buffer,
                         bvh_nodes_offset, copy_size);
    bvh_nodes_offset += copy_size;
  }

  // Update bindless set
  std::vector<VkDescriptorImageInfo> image_infos;
  image_infos.resize(vk_image_views.size());

  for (size_t i = 0; i < vk_image_views.size(); ++i) {
    VulkanImageView *image_view =
        p_resource_manager->access_image_view(vk_image_views[i]);
    image_infos[i] = VkDescriptorImageInfo{
        .sampler = p_resource_manager->access_sampler(vk_sampler)->vk_handle,
        .imageView = image_view->vk_handle,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  }
  VkWriteDescriptorSet image_write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = vk_bindless_texture_set,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = static_cast<u32>(vk_image_views.size()),
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = image_infos.data()};
  vkUpdateDescriptorSets(p_device->vk_device, 1, &image_write, 0, nullptr);
  staging_buffer.flush();
}

void Renderer::shutdown() {
  vkDeviceWaitIdle(p_device->vk_device);
  for (ImageViewHandle &view : vk_image_views) {
    p_resource_manager->queue_destroy({view, 0});
  }
  p_resource_manager->queue_destroy({vk_sampler, 0});
  p_resource_manager->queue_destroy({path_tracing_pipeline_handle, 0});
  p_resource_manager->queue_destroy({vk_scene_data_set_layout, 0});
  p_resource_manager->queue_destroy({vk_bindless_texture_set_layout, 0});
  p_resource_manager->queue_destroy({comp_shader_module, 0});
  p_resource_manager->queue_destroy({triangles_buffer, 0});
  p_resource_manager->queue_destroy({tri_ids_buffer, 0});
  p_resource_manager->queue_destroy({tlas_nodes_buffer, 0});
  p_resource_manager->queue_destroy({bvhs_buffer, 0});
  p_resource_manager->queue_destroy({bvh_nodes_buffer, 0});
  p_resource_manager->queue_destroy({lambert_buffer, 0});
  p_resource_manager->queue_destroy({metal_buffer, 0});
  p_resource_manager->queue_destroy({dielectric_buffer, 0});
  p_resource_manager->queue_destroy({uniform_buffer, 0});
  p_resource_manager->queue_destroy({image_buffer, 0});
  p_resource_manager->queue_destroy({final_image_view, 0});
  vkDestroyDescriptorPool(p_device->vk_device, vk_descriptor_pool, nullptr);
  vkDestroyDescriptorPool(p_device->vk_device, vk_bindless_descriptor_pool,
                          nullptr);
  staging_buffer.shutdown();
}

void Renderer::render(u64 frame_number) {
  // Rendering
  VkCommandBuffer cmd = p_device->get_current_cmd_buffer();
  p_device->push_debug_label("Compute Pass");
  VulkanImageView *vk_final_image_view =
      p_resource_manager->access_image_view(final_image_view);
  VulkanImage *vk_final_image =
      p_resource_manager->access_image(vk_final_image_view->image_handle);
  VkImageMemoryBarrier2 image_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_barrier.subresourceRange.baseMipLevel = 0;
  image_barrier.subresourceRange.levelCount = 1;
  image_barrier.subresourceRange.baseArrayLayer = 0;
  image_barrier.subresourceRange.layerCount = 1;
  image_barrier.image = vk_final_image->vk_handle;
  image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  image_barrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  image_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  if (reset_accumulation) {
    image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_CLEAR_BIT;
    image_barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency_info.dependencyFlags = 0;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &image_barrier;
    vkCmdPipelineBarrier2(cmd, &dependency_info);

    VkClearColorValue clear_color;
    clear_color.float32[0] = 0.f;
    clear_color.float32[1] = 0.f;
    clear_color.float32[2] = 0.f;
    clear_color.float32[3] = 0.f;
    vkCmdClearColorImage(cmd, vk_final_image->vk_handle,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1,
                         &image_barrier.subresourceRange);

    image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_CLEAR_BIT;
    image_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    // reset_accumulation = false;
  }

  // Pipeline Barrier
  image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  image_barrier.dstAccessMask =
      VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT;
  image_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency_info.dependencyFlags = 0;
  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers = &image_barrier;
  vkCmdPipelineBarrier2(cmd, &dependency_info);

  VulkanPipeline *path_tracing_pipeline =
      p_resource_manager->access_pipeline(path_tracing_pipeline_handle);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                    path_tracing_pipeline->vk_handle);

  ShaderPushConstant push_constant{};
  Vec3::set_float4(push_constant.pixel00_loc, pixel00_loc);
  Vec3::set_float4(push_constant.pixel_delta_u, pixel_delta_u);
  Vec3::set_float4(push_constant.pixel_delta_v, pixel_delta_v);
  Vec3::set_float4(push_constant.camera_center, center);
  push_constant.image_width = image_width;
  push_constant.image_height = image_height;
  std::mt19937 generator(std::random_device{}());
  push_constant.seed_value = generator();
  push_constant.triangle_count = (u32)triangles.size();
  push_constant.samples_per_pixel = SAMPLES_PER_FRAME;
  push_constant.pixel_samples_scale = pixel_samples_scale;
  push_constant.max_depth = max_depth;
  push_constant.defocus_angle = defocus_angle;
  Vec3::set_float4(push_constant.defocus_disk_u, defocus_disk_u);
  Vec3::set_float4(push_constant.defocus_disk_v, defocus_disk_v);

  const VkDescriptorSet sets[2] = {vk_scene_data_set, vk_bindless_texture_set};
  const VkBindDescriptorSetsInfo bind_info{
      .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      .layout = path_tracing_pipeline->vk_pipeline_layout,
      .firstSet = 0,
      .descriptorSetCount = 2,
      .pDescriptorSets = sets,
  };
  vkCmdBindDescriptorSets2(cmd, &bind_info);

  vkCmdPushConstants(cmd, path_tracing_pipeline->vk_pipeline_layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ShaderPushConstant),
                     &push_constant);

  vkCmdDispatch(cmd, image_width / 8, image_height / 8, 1);

  p_device->pop_debug_label();
}
