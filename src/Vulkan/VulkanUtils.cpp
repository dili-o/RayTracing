#include "VulkanUtils.hpp"
// Vendor
#include <filesystem>
#include <iostream>

namespace hlx {
std::string ToCompileStage(VkShaderStageFlagBits stage) {
  switch (stage) {
  case VK_SHADER_STAGE_VERTEX_BIT:
    return "vert";
  case VK_SHADER_STAGE_FRAGMENT_BIT:
    return "frag";
  case VK_SHADER_STAGE_COMPUTE_BIT:
    return "comp";
  case VK_SHADER_STAGE_TASK_BIT_EXT:
    return "task";
  case VK_SHADER_STAGE_MESH_BIT_EXT:
    return "mesh";
  default:
    HERROR("Unknown shader stage!");
    return nullptr;
  }
}

void HLX_API PrintVmaStats(VmaAllocator vmaAllocator, VkBool32) {
  char *statString = nullptr;
  vmaBuildStatsString(vmaAllocator, &statString, VK_TRUE);

  std::cout << statString << std::endl;

  vmaFreeStatsString(vmaAllocator, statString);
}

std::vector<char> ReadFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

bool CompileShader(const std::string &path, const std::string &shaderName,
                   const std::string &outputName, VkShaderStageFlagBits stage,
                   bool generateDebugSymbols) {
  std::filesystem::path originalPath = std::filesystem::current_path();
  std::filesystem::path shaderPath = path;

  cstring vulkanSdk = std::getenv("VULKAN_SDK");

  if (!vulkanSdk) {
    std::cerr << "VULKAN_SDK environment variable is not set.\n";
  }

  std::string compilerPath =
      std::string("\"") + vulkanSdk + "\\Bin\\slangc.exe\"";
  std::string args = " " + shaderName +
                     " -target spirv -profile spirv_1_3 -emit-spirv-directly "
                     "-fvk-use-entrypoint-name -entry computeMain " +
                     " -o Spirv/" + outputName;
  std::string command = compilerPath + args;
  if (generateDebugSymbols) {
    command += " -g";
  }

  try {
    std::filesystem::current_path(shaderPath);

    int result = std::system(command.c_str());

    if (result != 0) {
      std::ifstream errorFile("error.txt");
      std::string errorMsg((std::istreambuf_iterator<char>(errorFile)),
                           std::istreambuf_iterator<char>());
      std::cerr << "Compilation failed:\n" << errorMsg << '\n';
      return false;
    }

    // Clean up the error file regardless of success or failure
    std::remove("error.txt");

    std::filesystem::current_path(originalPath);

  } catch (const std::filesystem::filesystem_error &e) {
    std::cerr << "Filesystem error: " << e.what() << '\n';
    return false;
  }

  HINFO("Successfully compiled {} to {}", shaderName.c_str(),
        outputName.c_str());

  return true;
}

VkShaderModule HLX_API CreateShaderModule(VkDevice vkDevice,
                                          const std::vector<char> &code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  VK_CHECK(vkCreateShaderModule(vkDevice, &createInfo, nullptr, &shaderModule));
  return shaderModule;
}

static VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                         VK_DYNAMIC_STATE_SCISSOR};

namespace init {
VkImageCreateInfo ImageCreateInfo(VkExtent3D extents, u32 mipCount,
                                  VkFormat format,
                                  VkImageUsageFlags usageFlags) {
  VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent = extents;
  imageInfo.mipLevels = mipCount;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usageFlags;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  return imageInfo;
}

VkImageViewCreateInfo ImageViewCreateInfo(VkImage image, VkFormat format,
                                          VkImageAspectFlags aspectFlags,
                                          u32 mipLevels) {
  VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  return viewInfo;
}

VmaAllocationCreateInfo VmaAllocationInfo(VmaMemoryUsage usageFlags) {
  VmaAllocationCreateInfo allocationInfo{};
  allocationInfo.usage = usageFlags;
  return allocationInfo;
}

VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo() {
  VkPipelineDynamicStateCreateInfo dynamicState{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamicState.dynamicStateCount = 2;
  dynamicState.pDynamicStates = dynamicStates;
  return dynamicState;
}

VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo() {
  VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;
  return inputAssembly;
}

VkPipelineViewportStateCreateInfo PipelineViewportStateCreateInfo() {
  VkPipelineViewportStateCreateInfo viewportState{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  return viewportState;
}

VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo() {
  VkPipelineRasterizationStateCreateInfo rasterizer{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;
  return rasterizer;
}

VkPipelineMultisampleStateCreateInfo PipelineMultisampleStateCreateInfo() {
  VkPipelineMultisampleStateCreateInfo multisampling{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  return multisampling;
}

VkPipelineDepthStencilStateCreateInfo
PipelineDepthStencilStateCreateInfo(bool enabled) {
  VkPipelineDepthStencilStateCreateInfo depthStencil{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  depthStencil.depthTestEnable = enabled ? VK_TRUE : VK_FALSE;
  depthStencil.depthWriteEnable = enabled ? VK_TRUE : VK_FALSE;
  depthStencil.depthCompareOp =
      enabled ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_NEVER;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds = 0.0f;
  depthStencil.maxDepthBounds = 1.0f;
  return depthStencil;
}
} // namespace init

} // namespace hlx
