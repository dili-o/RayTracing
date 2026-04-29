#include "SkyRenderer.hpp"
#include "Camera.hpp"
#include "Core/Exceptions.hpp"
#include "Vulkan/VkDeviceManager.h"
#include "Vulkan/VkResourceManager.hpp"
#include "Vulkan/VkShaderCompilation.h"
#include "Vulkan/VkStagingBuffer.h"
#include "Vulkan/VkUtils.hpp"
// Vendor
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/trigonometric.hpp>
#include <stb_image.h>

namespace hlx {
void SkyRenderer::init(VkResourceManager *p_rm, SamplerHandle texture_sampler) {
  HASSERT(p_rm);
  HASSERT(p_rm->p_device);
  VkDeviceManager *p_device = p_rm->p_device;
  timer.start();

  // Constant buffers
  VmaAllocationCreateInfo vma_alloc_info{};
  vma_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
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

  // Bluenoise texture
  i32 width, height, channels;
  u8 *pixels = stbi_load(ASSETS_PATH "/Textures/bluenoise.png", &width, &height,
                         &channels, 4);
  HASSERT(pixels);
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_info.usage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  view_info.format = image_info.format;
  bluenoise_texture =
      p_rm->create_image_view("BluenoiseImageView", "BluenoiseImage",
                              image_info, vma_alloc_info, view_info);
  VkStagingBuffer staging_buffer;
  staging_buffer.init(
      p_device, p_rm,
      p_device->queue_family_indices.graphics_family_index.value(),
      p_device->vk_graphics_queue, width * height * 4);
  staging_buffer.stage(pixels, bluenoise_texture, width * height * 4);
  free(pixels);

  // Transition dummy texture
  // NOTE: Assumes the command buffer has not been started
  VkCommandBuffer cmd = staging_buffer.vk_command_buffer;

  VulkanImageView *dummy_view = p_rm->access_image_view(dummy_texture);
  VulkanImage *dummy_image = p_rm->access_image(dummy_view->image_handle);
  VkImageMemoryBarrier2 image_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
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

  staging_buffer.flush();
  staging_buffer.shutdown();

  // Sky cb set layout
  VkDescriptorSetLayoutCreateInfo layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};

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
  rendering_info.pColorAttachmentFormats =
      &p_rm->access_image(
               p_rm->access_image_view(transmittance_lut_texture)->image_handle)
           ->vk_format;
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

  VkDescriptorSetLayout set_layouts[2] = {
      p_rm->access_set_layout(sky_cb_set_layout)->vk_handle,
      p_rm->access_set_layout(sky_textures_set_layout)->vk_handle};

  VkPipelineLayoutCreateInfo pipeline_layout_info{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipeline_layout_info.setLayoutCount = 2;
  pipeline_layout_info.pSetLayouts = set_layouts;
  pipeline_layout_info.pPushConstantRanges = nullptr;
  pipeline_layout_info.pushConstantRangeCount = 0;

  transmittance_lut_pipeline = p_rm->create_graphics_pipeline(
      "TransmittanceLUTPipeline", pipeline_info, pipeline_layout_info);

  p_rm->queue_destroy({transmittance_lut_vs_shader});
  p_rm->queue_destroy({transmittance_lut_fs_shader});
} // namespace hlx

void SkyRenderer::shutdown(VkResourceManager *p_rm) {
  // Sky rendering shutdown
  p_rm->queue_destroy({bluenoise_texture});
  p_rm->queue_destroy({transmittance_lut_pipeline});
  p_rm->queue_destroy({sky_textures_set_layout});
  p_rm->queue_destroy({sky_cb_set_layout});

  p_rm->queue_destroy({dummy_texture});
  p_rm->queue_destroy({transmittance_lut_texture});

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    p_rm->queue_destroy({sky_constant_buffers[i]});
    p_rm->queue_destroy({sky_atmosphere_buffers[i]});
  }
}

void SkyRenderer::render(VkResourceManager *p_rm, Camera &camera,
                         u32 frame_index) {
  VkDeviceManager *p_device = p_rm->p_device;
  VkCommandBuffer cmd = p_device->get_current_cmd_buffer();
  // Sky update
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
}

} // namespace hlx
