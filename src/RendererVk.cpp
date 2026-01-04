#include "Renderer.hpp"
#include "Log.hpp"
#include "Vulkan/VulkanTypes.hpp"
// Vendor
#include <Vendor/renderdoc_app.h>

RENDERDOC_API_1_6_0 *rdoc_api = nullptr;

void InitRenderDocAPI() {
  if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
    pRENDERDOC_GetAPI RENDERDOC_GetAPI =
        (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
    if (RENDERDOC_GetAPI)
      RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void **)&rdoc_api);
  }
}

using namespace hlx;
static VkContext ctx;
static VkFormat imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
static VulkanImage final_image{};
static VulkanImageView final_image_view{};
static VulkanBuffer imageBuffer{};
static VkShaderModule comp_shader_module;
static VkPipeline vkPipeline;
static VulkanBuffer spheres_buffer{};
static VulkanBuffer triangles_buffer{};
static VulkanBuffer tri_ids_buffer{};
static VulkanBuffer bvh_nodes_buffer{};
static VulkanBuffer lambert_buffer{};
static VulkanBuffer metal_buffer{};
static VulkanBuffer dielectric_buffer{};
static VulkanBuffer uniformBuffer{};
static VkDescriptorSetLayout vkSetLayout;
static VkPipelineLayout vkPipelineLayout;
static VkSampler vkSampler;
static VkCommandPool vkCommandPool;
static VkCommandBuffer vkCommandBuffer;

static std::vector<VulkanImage>     vk_images;
static std::vector<VulkanImageView> vk_image_views;

struct UniformBuffer {
  VkDeviceAddress spheres;
  VkDeviceAddress triangles;
  VkDeviceAddress tri_ids;
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
  u32 sphere_count;
  u32 samples_per_pixel;

  f32 pixel_samples_scale;
  u32 max_depth;
  u32 triangle_count;
  f32 defocus_angle;

  f32 defocus_disk_u[4];
  f32 defocus_disk_v[4]; // 128
};

MaterialHandle RendererVk::add_lambert_material(const Vec3 &albedo) {
  vk_images.emplace_back(VulkanImage());
  vk_image_views.emplace_back(VulkanImageView());
  u8* pixels = static_cast<u8 *>(malloc(sizeof(u8) * BYTES_PER_PIXEL));
  HASSERT(pixels);
  pixels[0] = static_cast<u8>(albedo.x * 255.f);
  pixels[1] = static_cast<u8>(albedo.y * 255.f);
  pixels[2] = static_cast<u8>(albedo.z * 255.f);
  pixels[3] = 255;

  images.emplace_back(pixels, 1, 1);
  lambert_mats.push_back({static_cast<u32>(images.size() - 1)});
  return {MATERIAL_LAMBERT, ((u32)lambert_mats.size() - 1)};
}

MaterialHandle RendererVk::add_lambert_material(const std::string &filename) {
  vk_images.emplace_back(VulkanImage());
  vk_image_views.emplace_back(VulkanImageView());
  images.emplace_back(filename.c_str(), true);
  lambert_mats.push_back({static_cast<u32>(images.size() - 1)});
  return {MATERIAL_LAMBERT, ((u32)lambert_mats.size() - 1)};
}

MaterialHandle RendererVk::add_metal_material(const Vec3 &albedo,
                                              real fuzziness) {
  metal_mats.push_back({albedo.x, albedo.y, albedo.z, fuzziness});
  return {MATERIAL_METAL, ((u32)metal_mats.size() - 1)};
}

MaterialHandle RendererVk::add_dielectric_material(real refraction_index) {
  dielectric_mats.push_back({refraction_index});
  return {MATERIAL_DIELECTRIC, ((u32)dielectric_mats.size() - 1)};
}

void RendererVk::add_sphere(const Vec3 &origin, real radius,
                            MaterialHandle mat) {
  spheres.push_back(SphereGPU(origin, radius, mat.index, mat.type));
}

void RendererVk::add_triangle(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2,
														const Vec3 &n0, const Vec3 &n1, const Vec3 &n2,
														Vec2 uv_0, Vec2 uv_1, Vec2 uv_2,
														MaterialHandle mat_handle) {
  Vec3 centroid = (v0 + v1 + v2) * 0.3333f;
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

  triangles.push_back(TriangleGPU(v0, v1, v2, n0, n1, n2, uv_0, uv_1, uv_2, mat_handle));
}

void RendererVk::init(u32 image_width_, real aspect_ratio_,
                      u32 samples_per_pixel_, u32 max_depth_, real vfov_deg_) {
  initialize_camera(image_width_, aspect_ratio_, samples_per_pixel_, max_depth_,
                    vfov_deg_);

  ctx.Init();

  VkImageCreateInfo image_create_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image_create_info.imageType = VK_IMAGE_TYPE_2D;
  image_create_info.format = imageFormat;
  image_create_info.extent = {image_width, image_height, 1};
  image_create_info.mipLevels = 1;
  image_create_info.arrayLayers = 1;
  image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_create_info.usage =
      VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo vma_image_info{};
  vma_image_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  util::CreateVmaImage(ctx.vmaAllocator, image_create_info, vma_image_info,
                       final_image);

  VkImageViewCreateInfo image_view_create_info{
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  image_view_create_info.image = final_image.vkHandle;
  image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_create_info.format = imageFormat;
  image_view_create_info.subresourceRange.aspectMask =
      VK_IMAGE_ASPECT_COLOR_BIT;
  image_view_create_info.subresourceRange.baseMipLevel = 0;
  image_view_create_info.subresourceRange.levelCount = 1;
  image_view_create_info.subresourceRange.baseArrayLayer = 0;
  image_view_create_info.subresourceRange.layerCount = 1;
  util::CreateImageView(ctx.vkDevice, ctx.vkAllocationCallbacks,
                        image_view_create_info, final_image_view);

  // Pipeline
#ifdef _DEBUG
  bool d_symbols = true;
#else
  bool d_symbols = false;
#endif // _DEBUG
  std::string defines = "-DALBEDO_TEXTURE_COUNT=" + std::to_string(vk_image_views.size());
  if (!CompileShader(SHADER_PATH, "RayTracing.slang", "RayTracing.spv",
                     VK_SHADER_STAGE_COMPUTE_BIT, d_symbols, defines)) {
    HERROR("Failed to compile RayTracing.slang!");
  }

  std::vector<char> compShaderCode =
      ReadFile(SHADER_PATH "/Spirv/RayTracing.spv");
  comp_shader_module = CreateShaderModule(ctx.vkDevice, compShaderCode);

  VkPipelineShaderStageCreateInfo compShaderStageInfo{
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  compShaderStageInfo.module = comp_shader_module;
  compShaderStageInfo.pName = "computeMain";

  // Spheres buffer
  ctx.CreateVmaBuffer(spheres_buffer, sizeof(SphereGPU) * spheres.size(),
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_UNKNOWN, 0,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "SpheresBuffer");
  // Triangles buffer
  ctx.CreateVmaBuffer(triangles_buffer, sizeof(TriangleGPU) * triangles.size(),
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_UNKNOWN, 0,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TrianglesBuffer");
  // Triangle IDs buffer
  ctx.CreateVmaBuffer(tri_ids_buffer, sizeof(u32) * tri_ids.size(),
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_UNKNOWN, 0,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TrianglesIDsBuffer");

  // Lambert materials buffer
  ctx.CreateVmaBuffer(lambert_buffer, sizeof(GpuLambert) * lambert_mats.size(),
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_UNKNOWN, 0,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "LambertBuffer");
  // Metal materials buffer
  ctx.CreateVmaBuffer(metal_buffer, sizeof(GpuMetal) * metal_mats.size(),
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_UNKNOWN, 0,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "MetalBuffer");
  // Dielectric materials buffer
  ctx.CreateVmaBuffer(dielectric_buffer,
                      sizeof(GpuDielectric) * dielectric_mats.size(),
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_UNKNOWN, 0,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "DielectricBuffer");
  u32 max_depth = 0;
  bvh[0] = BVH(triangles.data(), triangles.size(), true, tri_ids, tri_centroids, max_depth);

  ctx.CreateVmaBuffer(bvh_nodes_buffer, sizeof(BVHNode) * bvh[0].bvh_nodes.size(),
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_UNKNOWN, 0,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BVHNodesBuffer");

  UniformBuffer uniform_buffer_data{};
  uniform_buffer_data.spheres = spheres_buffer.deviceAddress;
  uniform_buffer_data.triangles = triangles_buffer.deviceAddress;
  uniform_buffer_data.tri_ids = tri_ids_buffer.deviceAddress;
  uniform_buffer_data.bvh_nodes = bvh_nodes_buffer.deviceAddress;
  uniform_buffer_data.lambert_materials = lambert_buffer.deviceAddress;
  uniform_buffer_data.metal_materials = metal_buffer.deviceAddress;
  uniform_buffer_data.dielectric_materials = dielectric_buffer.deviceAddress;

  // Uniform buffer
  ctx.CreateVmaBuffer(uniformBuffer, sizeof(UniformBuffer),
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VMA_MEMORY_USAGE_UNKNOWN, 0,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "UniformBuffer");
  // Sampler
  VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	VK_CHECK(vkCreateSampler(ctx.vkDevice, &samplerInfo, nullptr, &vkSampler));

  // Descriptor Set layout
  VkDescriptorSetLayoutBinding outImageBinding{};
  outImageBinding.binding = 0;
  outImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  outImageBinding.descriptorCount = 1;
  outImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding uniformBinding{};
  uniformBinding.binding = 1;
  uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  uniformBinding.descriptorCount = 1;
  uniformBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding textureArray{};
  textureArray.binding = 2;
  textureArray.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  textureArray.descriptorCount = images.size();
  textureArray.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding bindings[3] = {outImageBinding, uniformBinding, textureArray};

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
  layoutInfo.bindingCount = ArraySize(bindings);
  layoutInfo.pBindings = bindings;
  VK_CHECK(vkCreateDescriptorSetLayout(
      ctx.vkDevice, &layoutInfo, ctx.vkAllocationCallbacks, &vkSetLayout));
  // Pipeline layout
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &vkSetLayout;

  VkPushConstantRange pushRange{};
  pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  pushRange.offset = 0;
  pushRange.size = sizeof(ShaderPushConstant);

  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushRange;
  VK_CHECK(vkCreatePipelineLayout(ctx.vkDevice, &pipelineLayoutInfo, nullptr,
                                  &vkPipelineLayout));
  // Pipeline
  VkComputePipelineCreateInfo pipelineCreateInfo{
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  pipelineCreateInfo.stage = compShaderStageInfo;
  pipelineCreateInfo.layout = vkPipelineLayout;
  VK_CHECK(vkCreateComputePipelines(ctx.vkDevice, VK_NULL_HANDLE, 1,
                                    &pipelineCreateInfo, nullptr, &vkPipeline));

  // Command Pool and Buffer
  VkCommandPoolCreateInfo commandPoolCreateInfo{
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  commandPoolCreateInfo.queueFamilyIndex =
      ctx.queueFamilyIndices.graphicsFamilyIndex.value();
  VK_CHECK(vkCreateCommandPool(ctx.vkDevice, &commandPoolCreateInfo, nullptr,
                               &vkCommandPool));
  VkCommandBufferAllocateInfo cbAllocInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cbAllocInfo.commandPool = vkCommandPool;
  cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbAllocInfo.commandBufferCount = 1;
  VK_CHECK(
      vkAllocateCommandBuffers(ctx.vkDevice, &cbAllocInfo, &vkCommandBuffer));
  // Transfer data to buffers
  ctx.CopyToBuffer(spheres_buffer.vkHandle, 0, spheres_buffer.size,
                   spheres.data(), vkCommandPool);
  ctx.CopyToBuffer(triangles_buffer.vkHandle, 0, triangles_buffer.size,
                   triangles.data(), vkCommandPool);
  ctx.CopyToBuffer(tri_ids_buffer.vkHandle, 0, tri_ids_buffer.size,
                   tri_ids.data(), vkCommandPool);
  ctx.CopyToBuffer(lambert_buffer.vkHandle, 0, lambert_buffer.size,
                   lambert_mats.data(), vkCommandPool);
  ctx.CopyToBuffer(metal_buffer.vkHandle, 0, metal_buffer.size,
                   metal_mats.data(), vkCommandPool);
  ctx.CopyToBuffer(dielectric_buffer.vkHandle, 0, dielectric_buffer.size,
                   dielectric_mats.data(), vkCommandPool);
  ctx.CopyToBuffer(bvh_nodes_buffer.vkHandle, 0, bvh_nodes_buffer.size,
                   bvh[0].bvh_nodes.data(), vkCommandPool);

  // Transfer data to Uniform buffer
  ctx.CopyToBuffer(uniformBuffer.vkHandle, 0, uniformBuffer.size,
                   &uniform_buffer_data, vkCommandPool);
  // Transfer data to images
	VulkanBuffer stagingBuffer{};
	ctx.CreateVmaBuffer(stagingBuffer, ctx.vk_11_properties.maxMemoryAllocationSize / 2, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
									VMA_MEMORY_USAGE_UNKNOWN, 0,
									VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
											VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	vmaMapMemory(ctx.vmaAllocator, stagingBuffer.vmaAllocation,
							 &stagingBuffer.pMappedData);


  for (size_t i = 0; i < images.size(); ++i) {
    VkImageCreateInfo imageInfo = init::ImageCreateInfo(
      { static_cast<u32>(images[i].width()),
      static_cast<u32>(images[i].height()),1 },
      1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		VmaAllocationCreateInfo vmaInfo =
      init::VmaAllocationInfo(VMA_MEMORY_USAGE_GPU_ONLY);
    util::CreateVmaImage(ctx.vmaAllocator, imageInfo, vmaInfo, vk_images[i]);

    VkImageViewCreateInfo viewInfo = init::ImageViewCreateInfo(vk_images[i].vkHandle,
      VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    util::CreateImageView(ctx.vkDevice, ctx.vkAllocationCallbacks, viewInfo, vk_image_views[i]);

		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		VK_CHECK(vkBeginCommandBuffer(vkCommandBuffer, &beginInfo));
    // Transition image to transfer dst
		VkImageMemoryBarrier2 image_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
		image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		image_barrier.srcAccessMask = VK_ACCESS_2_NONE;
		image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		image_barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier.image = vk_images[i].vkHandle;
		image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_barrier.subresourceRange.baseMipLevel = 0;
		image_barrier.subresourceRange.levelCount = 1;
		image_barrier.subresourceRange.baseArrayLayer = 0;
		image_barrier.subresourceRange.layerCount = 1;

		VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dependency_info.dependencyFlags = 0;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &image_barrier;

		vkCmdPipelineBarrier2(vkCommandBuffer, &dependency_info);

    size_t image_size = images[i].width() * images[i].height() * BYTES_PER_PIXEL;
		memcpy(stagingBuffer.pMappedData, images[i].pixel_data(0, 0), image_size);

		VkBufferImageCopy2 region{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};
		region.bufferOffset = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = {0, 0, 0};
    region.imageExtent = vk_images[i].vkExtents;

		VkCopyBufferToImageInfo2 buffer_image_info{
				VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2};
		buffer_image_info.srcBuffer = stagingBuffer.vkHandle;
    buffer_image_info.dstImage = vk_images[i].vkHandle;
    buffer_image_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		buffer_image_info.regionCount = 1;
		buffer_image_info.pRegions = &region;
		vkCmdCopyBufferToImage2(vkCommandBuffer, &buffer_image_info);

    // Transition image to shader read only
		image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		image_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		image_barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier2(vkCommandBuffer, &dependency_info);

    vkEndCommandBuffer(vkCommandBuffer);

		// Submition
		VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &vkCommandBuffer;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		VK_CHECK(vkQueueSubmit(ctx.vkGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

		VK_CHECK(vkQueueWaitIdle(ctx.vkGraphicsQueue));
    // TODO: Maybe free Image data since it's already in the GPU
  }

	vmaUnmapMemory(ctx.vmaAllocator, stagingBuffer.vmaAllocation);
	stagingBuffer.pMappedData = nullptr;

  util::DestroyBuffer(ctx.vkDevice, nullptr, stagingBuffer);
}

RendererVk::~RendererVk() {
  for (VulkanImage &image : vk_images) {
    util::DestroyVmaImage(ctx.vmaAllocator, image);
  }
  for (VulkanImageView &view : vk_image_views) {
    util::DestroyImageView(ctx.vkDevice, nullptr, view);
  }

  vkDestroySampler(ctx.vkDevice, vkSampler, nullptr);

  vkDestroyCommandPool(ctx.vkDevice, vkCommandPool, nullptr);
  vkDestroyPipelineLayout(ctx.vkDevice, vkPipelineLayout, nullptr);
  vkDestroyPipeline(ctx.vkDevice, vkPipeline, nullptr);
  vkDestroyDescriptorSetLayout(ctx.vkDevice, vkSetLayout, nullptr);
  vkDestroyShaderModule(ctx.vkDevice, comp_shader_module, nullptr);
  util::DestroyVmaBuffer(ctx.vmaAllocator, spheres_buffer);
  util::DestroyVmaBuffer(ctx.vmaAllocator, triangles_buffer);
  util::DestroyVmaBuffer(ctx.vmaAllocator, tri_ids_buffer);
  util::DestroyVmaBuffer(ctx.vmaAllocator, bvh_nodes_buffer);
  util::DestroyVmaBuffer(ctx.vmaAllocator, lambert_buffer);
  util::DestroyVmaBuffer(ctx.vmaAllocator, metal_buffer);
  util::DestroyVmaBuffer(ctx.vmaAllocator, dielectric_buffer);
  util::DestroyVmaBuffer(ctx.vmaAllocator, uniformBuffer);
  util::DestroyVmaBuffer(ctx.vmaAllocator, imageBuffer);
  util::DestroyVmaImage(ctx.vmaAllocator, final_image);
  util::DestroyImageView(ctx.vkDevice, ctx.vkAllocationCallbacks,
                         final_image_view);
  ctx.Shutdown();
}

void RendererVk::render(u8 *out_pixels) {
  // Rendering
  VkCommandBufferBeginInfo beginInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  VK_CHECK(vkBeginCommandBuffer(vkCommandBuffer, &beginInfo));
  InitRenderDocAPI();
  show_image = !rdoc_api;

  if (rdoc_api)
    rdoc_api->StartFrameCapture(nullptr, nullptr);

  // Pipeline Barrier
  VkImageMemoryBarrier2 imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  imageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
  imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageBarrier.image = final_image.vkHandle;
  imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageBarrier.subresourceRange.baseMipLevel = 0;
  imageBarrier.subresourceRange.levelCount = 1;
  imageBarrier.subresourceRange.baseArrayLayer = 0;
  imageBarrier.subresourceRange.layerCount = 1;

  VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependencyInfo.dependencyFlags = 0;
  dependencyInfo.imageMemoryBarrierCount = 1;
  dependencyInfo.pImageMemoryBarriers = &imageBarrier;
  vkCmdPipelineBarrier2(vkCommandBuffer, &dependencyInfo);

  vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    vkPipeline);

  std::vector<VkWriteDescriptorSet> writeSets;

  VkDescriptorImageInfo descritorImageInfo{};
  descritorImageInfo.sampler = VK_NULL_HANDLE;
  descritorImageInfo.imageView = final_image_view.vkHandle;
  descritorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkDescriptorBufferInfo descritorBufferInfo{};
  descritorBufferInfo.buffer = uniformBuffer.vkHandle;
  descritorBufferInfo.offset = 0;
  descritorBufferInfo.range = uniformBuffer.size;

  VkWriteDescriptorSet imageDescriptorWrite{
      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  imageDescriptorWrite.dstSet = VK_NULL_HANDLE;
  imageDescriptorWrite.dstBinding = 0;
  imageDescriptorWrite.dstArrayElement = 0;
  imageDescriptorWrite.descriptorCount = 1;
  imageDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  imageDescriptorWrite.pImageInfo = &descritorImageInfo;

  writeSets.push_back(imageDescriptorWrite);

  VkWriteDescriptorSet bufferDescriptorWrite{
      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  bufferDescriptorWrite.dstSet = VK_NULL_HANDLE;
  bufferDescriptorWrite.dstBinding = 1;
  bufferDescriptorWrite.dstArrayElement = 0;
  bufferDescriptorWrite.descriptorCount = 1;
  bufferDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bufferDescriptorWrite.pBufferInfo = &descritorBufferInfo;

  writeSets.push_back(bufferDescriptorWrite);

	imageDescriptorWrite.dstBinding = 2;
	imageDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  std::vector<VkDescriptorImageInfo> descriptorImageInfos;
  descriptorImageInfos.resize(vk_image_views.size());
  for (size_t i = 0; i < vk_image_views.size(); ++i) {
    VkDescriptorImageInfo& imageInfo = descriptorImageInfos[i];
		imageInfo.sampler = vkSampler;
		imageInfo.imageView = vk_image_views[i].vkHandle;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		imageDescriptorWrite.dstArrayElement = i;
		imageDescriptorWrite.pImageInfo = &descriptorImageInfos[i];

		writeSets.push_back(imageDescriptorWrite);
  }

  vkCmdPushDescriptorSetKHR(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            vkPipelineLayout, 0, writeSets.size(),
                            writeSets.data());

  ShaderPushConstant push_constant{};
  Vec3::set_float4(push_constant.pixel00_loc, pixel00_loc);
  Vec3::set_float4(push_constant.pixel_delta_u, pixel_delta_u);
  Vec3::set_float4(push_constant.pixel_delta_v, pixel_delta_v);
  Vec3::set_float4(push_constant.camera_center, center);
  push_constant.image_width = image_width;
  push_constant.image_height = image_height;
  push_constant.sphere_count = (u32)spheres.size();
  push_constant.triangle_count = (u32)triangles.size();
  push_constant.samples_per_pixel = samples_per_pixel;
  push_constant.pixel_samples_scale = pixel_samples_scale;
  push_constant.max_depth = max_depth;
  push_constant.defocus_angle = defocus_angle;
  Vec3::set_float4(push_constant.defocus_disk_u, defocus_disk_u);
  Vec3::set_float4(push_constant.defocus_disk_v, defocus_disk_v);

  vkCmdPushConstants(vkCommandBuffer, vkPipelineLayout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ShaderPushConstant),
                     &push_constant);

  vkCmdDispatch(vkCommandBuffer, image_width / 8, image_height / 8, 1);

  // Pipeline Barrier
  imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
  imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  imageBarrier.image = final_image.vkHandle;
  imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageBarrier.subresourceRange.baseMipLevel = 0;
  imageBarrier.subresourceRange.levelCount = 1;
  imageBarrier.subresourceRange.baseArrayLayer = 0;
  imageBarrier.subresourceRange.layerCount = 1;
  dependencyInfo.dependencyFlags = 0;
  dependencyInfo.imageMemoryBarrierCount = 1;
  dependencyInfo.pImageMemoryBarriers = &imageBarrier;
  vkCmdPipelineBarrier2(vkCommandBuffer, &dependencyInfo);

  // Copy result into buffer
  ctx.CreateVmaBuffer(imageBuffer, image_width * image_height * sizeof(f32) * 4,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_UNKNOWN, 0,
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                      "imageBuffer");
  VkBufferImageCopy2 copyRegion{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};
  copyRegion.bufferOffset = 0;
  copyRegion.bufferRowLength = 0;   // tightly packed
  copyRegion.bufferImageHeight = 0; // tightly packed
  copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copyRegion.imageSubresource.mipLevel = 0;
  copyRegion.imageSubresource.baseArrayLayer = 0;
  copyRegion.imageSubresource.layerCount = 1;
  copyRegion.imageOffset = {0, 0, 0};
  copyRegion.imageExtent = {image_width, image_height, 1};

  VkCopyImageToBufferInfo2 copyInfo{
      VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2};
  copyInfo.srcImage = final_image.vkHandle;
  copyInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  copyInfo.dstBuffer = imageBuffer.vkHandle;
  copyInfo.regionCount = 1;
  copyInfo.pRegions = &copyRegion;

  vkCmdCopyImageToBuffer2(vkCommandBuffer, &copyInfo);

  VK_CHECK(vkEndCommandBuffer(vkCommandBuffer));

  // Submition
  VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.waitSemaphoreCount = 0;
  submitInfo.pWaitSemaphores = nullptr;
  submitInfo.pWaitDstStageMask = nullptr;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &vkCommandBuffer;
  submitInfo.signalSemaphoreCount = 0;
  submitInfo.pSignalSemaphores = nullptr;

  VK_CHECK(vkQueueSubmit(ctx.vkGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

  VkResult res = vkQueueWaitIdle(ctx.vkGraphicsQueue);
  if (rdoc_api)
    rdoc_api->EndFrameCapture(nullptr, nullptr);

  VkDeviceFaultCountsEXT fault_counts{
      VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT};
  VkDeviceFaultInfoEXT fault_info{VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT};
  if (res == VK_ERROR_DEVICE_LOST) {
    vkGetDeviceFaultInfoEXT(ctx.vkDevice, &fault_counts, nullptr);

    vkGetDeviceFaultInfoEXT(ctx.vkDevice, &fault_counts, &fault_info);
    HTRACE("VkDeviceFaultInfoEXT.description: {}", fault_info.description);
  }

  // Copy mapped buffer to pixels array
  vmaMapMemory(ctx.vmaAllocator, imageBuffer.vmaAllocation,
               &imageBuffer.pMappedData);
  u32 index = 0;
  u32 bufferIndex = 0;
  for (u32 j = 0; j < image_height; j++) {
    for (u32 i = 0; i < image_width; i++) {
      f32 r = ((f32 *)imageBuffer.pMappedData)[bufferIndex++];
      f32 g = ((f32 *)imageBuffer.pMappedData)[bufferIndex++];
      f32 b = ((f32 *)imageBuffer.pMappedData)[bufferIndex++];
      bufferIndex++; // Extra increment because of 4 components

      r = linear_to_gamma(r);
      g = linear_to_gamma(g);
      b = linear_to_gamma(b);

      i32 ir = i32(255.99f * r);
      i32 ig = i32(255.99f * g);
      i32 ib = i32(255.99f * b);

      out_pixels[index++] = ir;
      out_pixels[index++] = ig;
      out_pixels[index++] = ib;
    }
  }
  vmaUnmapMemory(ctx.vmaAllocator, imageBuffer.vmaAllocation);
  imageBuffer.pMappedData = nullptr;
}

