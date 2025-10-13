#include "Log.hpp"
#include "Vulkan/VulkanTypes.hpp"
// Vendor
#include <Vendor/stb_image_write.h>
#include <iostream>

#define CHANNEL_NUM 3

int main(int argc, cstring *argv) {
  using namespace hlx;
  Logger logger;

  bool useCPU = false;
  bool useGPU = false;
  for (u32 i = 1; i < argc; ++i) {
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

  // Image
  u32 imageWidth = 256;
  u32 imageHeight = 256;
  u8 *pixels = new u8[imageWidth * imageHeight * CHANNEL_NUM];

  if (useCPU) {
    // Render
    u32 index = 0;
    for (int j = 0; j < imageHeight; j++) {
      std::clog << "\rScanlines remaining: " << (imageHeight - j) << ' '
                << std::flush;
      for (int i = 0; i < imageWidth; i++) {
        f64 r = f64(i) / (imageWidth - 1);
        f64 g = f64(j) / (imageHeight - 1);
        f64 b = 0.0;
        i32 ir = i32(255.99 * r);
        i32 ig = i32(255.99 * g);
        i32 ib = i32(255.99 * b);

        pixels[index++] = ir;
        pixels[index++] = ig;
        pixels[index++] = ib;
      }
    }
    std::clog << "\rDone.                 \n";
  } else {
    VkContext ctx;
    ctx.Init();

    VkFormat imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    VkImageCreateInfo imageCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = imageFormat;
    imageCreateInfo.extent = {imageWidth, imageHeight, 1};
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
    // Descriptor Set layout
    VkDescriptorSetLayout vkSetLayout;
    VkDescriptorSetLayoutBinding outImageBinding{};
    outImageBinding.binding = 0;
    outImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outImageBinding.descriptorCount = 1;
    outImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &outImageBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(
        ctx.vkDevice, &layoutInfo, ctx.vkAllocationCallbacks, &vkSetLayout));
    // Pipeline layout
    VkPipelineLayout vkPipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vkSetLayout;
    struct ShaderPushConstant {
      u32 imageWidth;
      u32 imageHeight;
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

    // Rendering
    VkCommandBufferBeginInfo beginInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(vkCommandBuffer, &beginInfo));

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

    VkWriteDescriptorSet descriptorWrite{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet = VK_NULL_HANDLE;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrite.pImageInfo = &descritorImageInfo;
    vkCmdPushDescriptorSetKHR(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                              vkPipelineLayout, 0, 1, &descriptorWrite);

    ShaderPushConstant pushConstant{imageWidth, imageHeight};

    vkCmdPushConstants(vkCommandBuffer, vkPipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(ShaderPushConstant), &pushConstant);

    vkCmdDispatch(vkCommandBuffer, imageWidth / 8, imageHeight / 8, 1);

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
    util::CreateVmaBuffer(ctx.vmaAllocator, ctx.vkDevice, imageBuffer,
                          imageWidth * imageHeight * 4 * 4,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VMA_MEMORY_USAGE_UNKNOWN, 0,
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    VkBufferImageCopy2 copyRegion{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;   // tightly packed
    copyRegion.bufferImageHeight = 0; // tightly packed
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {imageWidth, imageHeight, 1};

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

    // Copy mapped buffer to pixels array
    vmaMapMemory(ctx.vmaAllocator, imageBuffer.vmaAllocation,
                 &imageBuffer.pMappedData);
    u32 index = 0;
    u32 bufferIndex = 0;
    for (int j = 0; j < imageHeight; j++) {
      for (int i = 0; i < imageWidth; i++) {
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
    util::DestroyVmaBuffer(ctx.vmaAllocator, imageBuffer);
    util::DestroyVmaImage(ctx.vmaAllocator, image);
    util::DestroyImageView(ctx.vkDevice, ctx.vkAllocationCallbacks, imageView);
    ctx.Shutdown();
  }

  if (!stbi_write_png("image.png", imageWidth, imageHeight, CHANNEL_NUM, pixels,
                      imageWidth * CHANNEL_NUM)) {
    HERROR("Failed to write to file: {}", "image.png");
    return 1;
  }
}
