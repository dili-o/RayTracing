#include "PathTracer.hpp"
#include "Core/Event.hpp"
#include "Core/Exceptions.hpp"
#include "Core/Input.hpp"
#include "Platform/Platform.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"
#include "Vulkan/VkPipelineStates.hpp"
#include "Vulkan/VkResources.hpp"
#include "Vulkan/VkShaderCompilation.h"
// Vendor
#include <filesystem>

namespace hlx {
bool application_on_resize_event(u16 event_code, void *sender, void *listener,
                                 EventContext context) {
  PathTracer *app = (PathTracer *)listener;
  app->device.reset();
  return false;
}

bool application_on_event(u16 event_code, void *sender, void *listener,
                          EventContext context) {
  PathTracer *app = (PathTracer *)listener;
  switch (event_code) {
  case SDL_EVENT_QUIT: {
    app->end_application = true;
    return true;
  } break;
  }
  return false;
}

bool application_on_key(u16 event_code, void *sender, void *listener,
                        EventContext context) {
  switch (event_code) {
  case SDL_EVENT_KEY_DOWN: {
    u16 key_code = context.data.u16[0];
    if (key_code == SDL_SCANCODE_ESCAPE) {
      EventContext context{};
      EventSys::fire_event(SDL_EVENT_QUIT, 0, context);
      return true;
    }
  } break;
  }
  return false;
}

void PathTracer::init() {
  PlatformConfiguration config{
      .width = 1280, .height = 720, .name = "Path Tracer"};
  Platform::init(config);
  InputSys::init();
  EventSys::init();
  EventSys::register_event(SDL_EVENT_WINDOW_RESIZED, this,
                           application_on_resize_event);
  EventSys::register_event(SDL_EVENT_QUIT, this, application_on_event);
  EventSys::register_event(SDL_EVENT_KEY_DOWN, 0, application_on_key);
  EventSys::register_event(SDL_EVENT_KEY_UP, 0, application_on_key);
  device.init();
  resource_manager.init(&device);
  SlangCompiler::init();

  end_application = false;
}

void PathTracer::run() {
  Renderer renderer;
  renderer.p_resource_manager = &resource_manager;
  renderer.p_device = &device;
  renderer.staging_buffer.init(
      &device, &resource_manager,
      device.queue_family_indices.graphics_family_index.value(),
      device.vk_graphics_queue, 100'000'000);
  std::filesystem::path scene_path =
      "D:/raytracing-in-one-weekend/Scenes/CubeWorld.json";
  load_scene(scene_path, &renderer);

  ShaderHandle vert_shader;
  ShaderHandle frag_shader;
  PipelineHandle full_screen;

  ShaderBlob blob;
  try {
    SlangCompiler::compile_code("vertMain", "Fullscreen",
                                ASSETS_PATH "/Shaders/Fullscreen.slang", blob);
    vert_shader = resource_manager.create_shader("FullscreenVert", blob);

    SlangCompiler::compile_code("fragMain", "Fullscreen",
                                ASSETS_PATH "/Shaders/Fullscreen.slang", blob);
    frag_shader = resource_manager.create_shader("FullscreenFrag", blob);

  } catch (Exception exception) {
    HERROR("{}", exception.what());
  }

  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layout_info.bindingCount = 1;
  layout_info.pBindings = &binding;
  SetLayoutHandle final_image_set_layout =
      resource_manager.create_descriptor_set_layout("FinalImageSetLayout",
                                                    layout_info);

  const VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = device.vk_descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts =
          &resource_manager.access_set_layout(final_image_set_layout)
               ->vk_handle};

  VkDescriptorSet final_image_set;
  VK_CHECK(vkAllocateDescriptorSets(device.vk_device, &alloc_info,
                                    &final_image_set));
  VkDescriptorImageInfo image_info = {
      .sampler =
          resource_manager.access_sampler(renderer.vk_sampler)->vk_handle,
      .imageView =
          resource_manager.access_image_view(renderer.final_image_view)
              ->vk_handle,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write_info{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = final_image_set,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &image_info};

  vkUpdateDescriptorSets(device.vk_device, 1, &write_info, 0, nullptr);

  // Create the pipeline
  const VkPipelineRasterizationStateCreateInfo raster_info =
      VkRasterizerStates::no_cull_info();
  const VkPipelineDepthStencilStateCreateInfo depth_stenci_info =
      VkDepthStencilStates::depth_disabled_info();
  const VkPipelineRenderingCreateInfo rendering_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &device.swapchain.vk_surface_format.format,
      .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED};
  const std::array<VkPipelineShaderStageCreateInfo, 2> shader_infos = {
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = resource_manager.access_shader(vert_shader)->vk_handle,
          .pName = "main",
          .pSpecializationInfo = nullptr},
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = resource_manager.access_shader(frag_shader)->vk_handle,
          .pName = "main",
          .pSpecializationInfo = nullptr}};
  const VkPipelineColorBlendAttachmentState color_blend_attachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
  const VkPipelineColorBlendStateCreateInfo color_blend_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_CLEAR,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment};

  const VkPipelineVertexInputStateCreateInfo vertex_input_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = nullptr};
  const VkPipelineMultisampleStateCreateInfo multi_sample_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE};
  const VkPipelineInputAssemblyStateCreateInfo input_assembly_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE};
  const VkPipelineViewportStateCreateInfo viewport_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};
  static std::array<VkDynamicState, 2> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  const VkPipelineDynamicStateCreateInfo dynamic_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = dynamic_states.size(),
      .pDynamicStates = dynamic_states.data()};

  VkGraphicsPipelineCreateInfo pipeline_info{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pipeline_info.pRasterizationState = &raster_info;
  pipeline_info.pDepthStencilState = &depth_stenci_info;
  pipeline_info.pNext = &rendering_info;
  pipeline_info.stageCount = shader_infos.size();
  pipeline_info.pStages = shader_infos.data();
  pipeline_info.pColorBlendState = &color_blend_info;
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pMultisampleState = &multi_sample_info;
  pipeline_info.pInputAssemblyState = &input_assembly_info;
  pipeline_info.pDynamicState = &dynamic_info;
  pipeline_info.pViewportState = &viewport_info;

  VkPushConstantRange push_constant = {.stageFlags =
                                           VK_SHADER_STAGE_FRAGMENT_BIT,
                                       .offset = 0,
                                       .size = sizeof(u32)};
  const VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts =
          &resource_manager.access_set_layout(final_image_set_layout)
               ->vk_handle,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant};

  full_screen = resource_manager.create_graphics_pipeline(
      "Fullscreen", pipeline_info, pipeline_layout_info);

  u64 frame_number = 0;
  while (!end_application) {
    Platform::handle_os_messages();

    if (!Platform::is_suspended()) {
      device.begin_frame();
      VkCommandBuffer cmd = device.get_current_cmd_buffer();

      renderer.render(0);

      // Transition the final image to sampled layout
      VulkanImageView *vk_final_image_view =
          resource_manager.access_image_view(renderer.final_image_view);
      VulkanImage *vk_final_image =
          resource_manager.access_image(vk_final_image_view->image_handle);
      VkImageMemoryBarrier2 image_barrier{
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
      image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      image_barrier.subresourceRange.baseMipLevel = 0;
      image_barrier.subresourceRange.levelCount = 1;
      image_barrier.subresourceRange.baseArrayLayer = 0;
      image_barrier.subresourceRange.layerCount = 1;
      image_barrier.image = vk_final_image->vk_handle;
      image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      image_barrier.srcAccessMask =
          VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT;
      image_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;

      image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      image_barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
      image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
      dependency_info.dependencyFlags = 0;
      dependency_info.imageMemoryBarrierCount = 1;
      dependency_info.pImageMemoryBarriers = &image_barrier;
      vkCmdPipelineBarrier2(cmd, &dependency_info);

      const VkExtent2D screen_extents{.width = device.back_buffer_width,
                                      .height = device.back_buffer_height};

      VkRenderingAttachmentInfo color_attachment_info{
          VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
      color_attachment_info.imageView = device.get_current_backbuffer_view();
      color_attachment_info.imageLayout =
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      color_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;

      VkRenderingInfo render_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
      render_info.layerCount = 1;
      render_info.renderArea = {{0, 0}, screen_extents};
      render_info.viewMask = 0;
      render_info.colorAttachmentCount = 1;
      render_info.pColorAttachments = &color_attachment_info;
      render_info.pDepthAttachment = nullptr;
      render_info.pStencilAttachment = nullptr;
      vkCmdBeginRendering(cmd, &render_info);

      const VkRect2D scissor{.offset = {0, 0}, .extent = screen_extents};
      vkCmdSetScissor(cmd, 0, 1, &scissor);
      const VkViewport viewport{
          .x = 0.f,
          .y = 0.f,
          .width = static_cast<float>(screen_extents.width),
          .height = static_cast<float>(screen_extents.height),
          .minDepth = 0.f,
          .maxDepth = 1.f};
      vkCmdSetViewport(cmd, 0, 1, &viewport);

      vkCmdBindPipeline(
          cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
          resource_manager.access_pipeline(full_screen)->vk_handle);

      const VkBindDescriptorSetsInfo bind_info{
          .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .layout =
              resource_manager.access_pipeline(full_screen)->vk_pipeline_layout,
          .firstSet = 0,
          .descriptorSetCount = 1,
          .pDescriptorSets = &final_image_set,
      };
      vkCmdBindDescriptorSets2(cmd, &bind_info);
      float total_rays =
          float(frame_number + 1) * float(renderer.samples_per_pixel);
      vkCmdPushConstants(
          cmd,
          resource_manager.access_pipeline(full_screen)->vk_pipeline_layout,
          VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &total_rays);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRendering(cmd);

      device.end_frame();
      device.present();
      ++frame_number;
    }
  }

  renderer.shutdown();
  resource_manager.queue_destroy({final_image_set_layout, 0});
  resource_manager.queue_destroy({full_screen, 0});
  resource_manager.queue_destroy({vert_shader, 0});
  resource_manager.queue_destroy({frag_shader, 0});

  vkDeviceWaitIdle(device.vk_device);
  // Deletes all remaining resources
  resource_manager.update(UINT64_MAX);
}

void PathTracer::shutdown() {
  SlangCompiler::shutdown();
  resource_manager.shutdown();
  device.shutdown();
  InputSys::shutdown();
  EventSys::unregister_event(SDL_EVENT_WINDOW_RESIZED, this,
                             application_on_resize_event);
  EventSys::unregister_event(SDL_EVENT_QUIT, this, application_on_event);
  EventSys::unregister_event(SDL_EVENT_KEY_DOWN, 0, application_on_key);
  EventSys::unregister_event(SDL_EVENT_KEY_UP, 0, application_on_key);
  EventSys::shutdown();
  Platform::shutdown();
}

} // namespace hlx
