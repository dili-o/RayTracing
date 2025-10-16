#include "Camera.hpp"
#include "Defines.hpp"
#include "HittableList.hpp"
#include "Log.hpp"
#include "Sphere.hpp"
#include "Vulkan/VulkanTypes.hpp"
// Vendor
#include <Vendor/renderdoc_app.h>
#include <Vendor/stb_image_write.h>

RENDERDOC_API_1_6_0 *rdoc_api = nullptr;

void InitRenderDocAPI() {
  if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
    pRENDERDOC_GetAPI RENDERDOC_GetAPI =
        (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
    if (RENDERDOC_GetAPI)
      RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void **)&rdoc_api);
  }
}

#define CHANNEL_NUM 3

static cstring image_cpu = "image_cpu.png";
static cstring image_gpu = "image_gpu.png";

int main(int argc, cstring *argv) {
  using namespace hlx;
  Logger logger;

  bool useCPU = false;
  bool useGPU = false;
  for (i32 i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "-cpu")) {
      useCPU = true;
    } else if (!strcmp(argv[i], "-gpu")) {
      useGPU = true;
    }
  }

  if (useCPU && useGPU) {
    HERROR("Cannot enable both -cpu and -gpu, select only one!");
    return 1;
  } else if (!useCPU && !useGPU) {
    HERROR("Must enable either -cpu or -gpu!");
    return 1;
  }

  // World
  HittableList world;
  world.add(make_shared<Sphere>(Point3(0.f, 0.f, -1.f), 0.5f));
  world.add(make_shared<Sphere>(Point3(0.f, -100.5f, -1), 100.f));

  std::vector<GpuSphere> spheres;
  spheres.push_back(GpuSphere(Point3(0.f, 0.f, -1.f), 0.5f));
  spheres.push_back(GpuSphere(Point3(0.f, -100.5f, -1.f), 100.f));

  Camera camera;
  camera.initialize(384, 16.f / 9.f, 30);

  u8 *pixels = new u8[camera.image_width * camera.image_height * CHANNEL_NUM];
  std::chrono::steady_clock::time_point start;

  if (useCPU) {
    start = std::chrono::high_resolution_clock::now();
    // Render
    camera.render(world, pixels);
    std::clog << "\rDone.                 \n";
  } else {
    start = std::chrono::high_resolution_clock::now();
    VkContext ctx;
    ctx.Init();

    VkFormat imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    VkImageCreateInfo imageCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = imageFormat;
    imageCreateInfo.extent = {camera.image_width, camera.image_height, 1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo vmaImageInfo{};
    vmaImageInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VulkanImage image{};
    util::CreateVmaImage(ctx.vmaAllocator, imageCreateInfo, vmaImageInfo,
                         image);

    VkImageViewCreateInfo imageViewCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewCreateInfo.image = image.vkHandle;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = imageFormat;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    VulkanImageView imageView{};
    util::CreateImageView(ctx.vkDevice, ctx.vkAllocationCallbacks,
                          imageViewCreateInfo, imageView);

    // Pipeline
    if (!CompileShader(SHADER_PATH, "RayTracing.slang", "RayTracing.spv",
                       VK_SHADER_STAGE_COMPUTE_BIT, false)) {
      HERROR("Failed to compile RayTracing.slang!");
    }

    std::vector<char> compShaderCode =
        ReadFile(SHADER_PATH "/Spirv/RayTracing.spv");
    VkShaderModule compShaderModule =
        CreateShaderModule(ctx.vkDevice, compShaderCode);

    VkPipelineShaderStageCreateInfo compShaderStageInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compShaderStageInfo.module = compShaderModule;
    compShaderStageInfo.pName = "computeMain";

    // Spheres buffer
    VulkanBuffer spheresBuffer{};
    util::CreateVmaBuffer(ctx.vmaAllocator, ctx.vkDevice, spheresBuffer,
                          sizeof(GpuSphere) * spheres.size(),
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                          VMA_MEMORY_USAGE_UNKNOWN, 0,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ctx.SetResourceName(VK_OBJECT_TYPE_BUFFER, (u64)spheresBuffer.vkHandle,
                        "spheresBuffer");
    // Uniform buffer
    VulkanBuffer uniformBuffer{};
    util::CreateVmaBuffer(
        ctx.vmaAllocator, ctx.vkDevice, uniformBuffer, sizeof(VkDeviceAddress),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_UNKNOWN, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ctx.SetResourceName(VK_OBJECT_TYPE_BUFFER, (u64)uniformBuffer.vkHandle,
                        "uniformBuffer");

    // Descriptor Set layout
    VkDescriptorSetLayout vkSetLayout;
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

    VkDescriptorSetLayoutBinding bindings[2] = {outImageBinding,
                                                uniformBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    layoutInfo.bindingCount = ArraySize(bindings);
    layoutInfo.pBindings = bindings;
    VK_CHECK(vkCreateDescriptorSetLayout(
        ctx.vkDevice, &layoutInfo, ctx.vkAllocationCallbacks, &vkSetLayout));
    // Pipeline layout
    VkPipelineLayout vkPipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vkSetLayout;

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
      f32 padding[3];
    };

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(ShaderPushConstant);

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(ctx.vkDevice, &pipelineLayoutInfo, nullptr,
                                    &vkPipelineLayout));
    // Pipeline
    VkPipeline vkPipeline;
    VkComputePipelineCreateInfo pipelineCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineCreateInfo.stage = compShaderStageInfo;
    pipelineCreateInfo.layout = vkPipelineLayout;
    VK_CHECK(vkCreateComputePipelines(ctx.vkDevice, VK_NULL_HANDLE, 1,
                                      &pipelineCreateInfo, nullptr,
                                      &vkPipeline));

    // Command Pool and Buffer
    VkCommandPool vkCommandPool;
    VkCommandPoolCreateInfo commandPoolCreateInfo{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolCreateInfo.flags =
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex =
        ctx.queueFamilyIndices.graphicsFamilyIndex.value();
    VK_CHECK(vkCreateCommandPool(ctx.vkDevice, &commandPoolCreateInfo, nullptr,
                                 &vkCommandPool));
    VkCommandBuffer vkCommandBuffer;
    VkCommandBufferAllocateInfo cbAllocInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbAllocInfo.commandPool = vkCommandPool;
    cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAllocInfo.commandBufferCount = 1;
    VK_CHECK(
        vkAllocateCommandBuffers(ctx.vkDevice, &cbAllocInfo, &vkCommandBuffer));
    // Transfer data to Spheres buffer
    ctx.CopyToBuffer(spheresBuffer.vkHandle, 0, spheresBuffer.size,
                     spheres.data(), vkCommandPool);
    // Transfer data to Uniform buffer
    ctx.CopyToBuffer(uniformBuffer.vkHandle, 0, uniformBuffer.size,
                     &spheresBuffer.deviceAddress, vkCommandPool);

    // Rendering
    VkCommandBufferBeginInfo beginInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(vkCommandBuffer, &beginInfo));
    InitRenderDocAPI();
    if (rdoc_api)
      rdoc_api->StartFrameCapture(nullptr, nullptr);

    // Pipeline Barrier
    VkImageMemoryBarrier2 imageBarrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarrier.image = image.vkHandle;
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
    VkDescriptorImageInfo descritorImageInfo{};
    descritorImageInfo.sampler = VK_NULL_HANDLE;
    descritorImageInfo.imageView = imageView.vkHandle;
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

    VkWriteDescriptorSet bufferDescriptorWrite{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    bufferDescriptorWrite.dstSet = VK_NULL_HANDLE;
    bufferDescriptorWrite.dstBinding = 1;
    bufferDescriptorWrite.dstArrayElement = 0;
    bufferDescriptorWrite.descriptorCount = 1;
    bufferDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bufferDescriptorWrite.pBufferInfo = &descritorBufferInfo;

    VkWriteDescriptorSet descriptorWrites[2] = {imageDescriptorWrite,
                                                bufferDescriptorWrite};
    vkCmdPushDescriptorSetKHR(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                              vkPipelineLayout, 0, ArraySize(descriptorWrites),
                              descriptorWrites);

    ShaderPushConstant pushConstant{};
    Vec3::set_float4(pushConstant.pixel00_loc, camera.pixel00_loc);
    Vec3::set_float4(pushConstant.pixel_delta_u, camera.pixel_delta_u);
    Vec3::set_float4(pushConstant.pixel_delta_v, camera.pixel_delta_v);
    Vec3::set_float4(pushConstant.camera_center, camera.center);
    pushConstant.image_width = camera.image_width;
    pushConstant.image_height = camera.image_height;
    pushConstant.sphere_count = (u32)world.objects.size();
    pushConstant.samples_per_pixel = camera.samples_per_pixel;
    pushConstant.pixel_samples_scale = camera.pixel_samples_scale;

    vkCmdPushConstants(vkCommandBuffer, vkPipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(ShaderPushConstant), &pushConstant);

    vkCmdDispatch(vkCommandBuffer, camera.image_width / 8,
                  camera.image_height / 8, 1);

    // Pipeline Barrier
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageBarrier.image = image.vkHandle;
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
    VulkanBuffer imageBuffer{};
    util::CreateVmaBuffer(
        ctx.vmaAllocator, ctx.vkDevice, imageBuffer,
        camera.image_width * camera.image_height * sizeof(f32) * 4,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_UNKNOWN, 0,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    ctx.SetResourceName(VK_OBJECT_TYPE_BUFFER, (u64)imageBuffer.vkHandle,
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
    copyRegion.imageExtent = {camera.image_width, camera.image_height, 1};

    VkCopyImageToBufferInfo2 copyInfo{
        VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2};
    copyInfo.srcImage = image.vkHandle;
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

    VK_CHECK(
        vkQueueSubmit(ctx.vkGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

    vkQueueWaitIdle(ctx.vkGraphicsQueue);
    if (rdoc_api)
      rdoc_api->EndFrameCapture(nullptr, nullptr);

    // Copy mapped buffer to pixels array
    vmaMapMemory(ctx.vmaAllocator, imageBuffer.vmaAllocation,
                 &imageBuffer.pMappedData);
    u32 index = 0;
    u32 bufferIndex = 0;
    for (u32 j = 0; j < camera.image_height; j++) {
      for (u32 i = 0; i < camera.image_width; i++) {
        f32 r = ((f32 *)imageBuffer.pMappedData)[bufferIndex++];
        f32 g = ((f32 *)imageBuffer.pMappedData)[bufferIndex++];
        f32 b = ((f32 *)imageBuffer.pMappedData)[bufferIndex++];
        bufferIndex++; // Extra increment because of 4 components

        i32 ir = i32(255.99f * r);
        i32 ig = i32(255.99f * g);
        i32 ib = i32(255.99f * b);

        pixels[index++] = ir;
        pixels[index++] = ig;
        pixels[index++] = ib;
      }
    }
    vmaUnmapMemory(ctx.vmaAllocator, imageBuffer.vmaAllocation);
    imageBuffer.pMappedData = nullptr;

    // Destruction
    vkDestroyCommandPool(ctx.vkDevice, vkCommandPool, nullptr);
    vkDestroyPipelineLayout(ctx.vkDevice, vkPipelineLayout, nullptr);
    vkDestroyPipeline(ctx.vkDevice, vkPipeline, nullptr);
    vkDestroyDescriptorSetLayout(ctx.vkDevice, vkSetLayout, nullptr);
    vkDestroyShaderModule(ctx.vkDevice, compShaderModule, nullptr);
    util::DestroyVmaBuffer(ctx.vmaAllocator, spheresBuffer);
    util::DestroyVmaBuffer(ctx.vmaAllocator, uniformBuffer);
    util::DestroyVmaBuffer(ctx.vmaAllocator, imageBuffer);
    util::DestroyVmaImage(ctx.vmaAllocator, image);
    util::DestroyImageView(ctx.vkDevice, ctx.vkAllocationCallbacks, imageView);
    ctx.Shutdown();
  }

  auto end = std::chrono::high_resolution_clock::now();

  double seconds = std::chrono::duration<double>(end - start).count();
  std::cout << "Total time: " << seconds << " seconds\n";

  if (!stbi_write_png(useCPU ? image_cpu : image_gpu, camera.image_width,
                      camera.image_height, CHANNEL_NUM, pixels,
                      camera.image_width * CHANNEL_NUM)) {
    HERROR("Failed to write to file: {}", "image.png");
    return 1;
  }
  if (!rdoc_api)
    ShellExecuteA(nullptr,                        // parent window (none)
                  "open",                         // operation
                  useCPU ? image_cpu : image_gpu, // file to open
                  nullptr,                        // parameters (none)
                  nullptr,                        // default directory
                  SW_SHOW                         // show the window
    );
}
