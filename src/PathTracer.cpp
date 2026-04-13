#include "PathTracer.hpp"
#include "Camera.hpp"
#include "Core/Clock.hpp"
#include "Core/Event.hpp"
#include "Core/Exceptions.hpp"
#include "Core/Input.hpp"
#include "Platform/Platform.hpp"
#include "SceneGraph.hpp"
#include "Vulkan/VkPipelineStates.hpp"
#include "Vulkan/VkResources.hpp"
#include "Vulkan/VkShaderCompilation.h"
#include "Vulkan/VkUtils.hpp"
// Vendor
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <imgui/imgui.h>

namespace hlx {

static SceneGraph scene_graph = SceneGraph(100);
static u32 selected_node_id = INVALID_NODE_ID;

bool application_on_resize_event(u16 event_code, void *sender, void *listener,

                                 EventContext context) {
  PathTracer *app = (PathTracer *)listener;
  app->resize();
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
  PathTracer *app = static_cast<PathTracer *>(listener);
  if (event_code == SDL_EVENT_KEY_DOWN) {
    u16 scan_code = context.data.u16[0];
    switch (scan_code) {
    case SDL_SCANCODE_ESCAPE: {
      EventContext context{};
      EventSys::fire_event(SDL_EVENT_QUIT, 0, context);
      return true;
    }
    case SDL_SCANCODE_P: {
      app->device.set_vsync(!app->device.vsync_enabled);
      return false;
    }
    default:
      break;
    }
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
  EventSys::register_event(SDL_EVENT_KEY_DOWN, this, application_on_key);
  EventSys::register_event(SDL_EVENT_KEY_UP, this, application_on_key);
  device.init();
  rm.init(&device);
  staging_buffer.init(&device, &rm,
                      device.queue_family_indices.transfer_family_index.value(),
                      device.vk_transfer_queue, 500'000);
  SlangCompiler::init();
  renderer.init(&device, &rm, config.width, config.height);

  scene_ui.init(&device, &rm, staging_buffer);

  end_application = false;
  staging_buffer.flush();

  u32 root_id = scene_graph.add_node(INVALID_NODE_ID, 0, "Root");
  for (u32 i = 0; i < 5; ++i) {
    scene_graph.add_node(root_id, 1, std::string());
  }

  for (u32 i = 0; i < 3; ++i) {
    scene_graph.add_node(2, 1, std::string());
  }
  struct Transform {
    glm::vec3 position{0.f};
    glm::vec4 rotation = glm::vec4(0.f, 1.f, 0.f, 0.f);
    glm::vec3 scale{1.f};

    glm::mat4 get_mat4() {
      return glm::translate(glm::mat4(1.f), position) *
             glm::rotate(glm::mat4(1.f), rotation.w, glm::vec3(rotation)) *
             glm::scale(glm::mat4(1.f), scale);
    }
  };
  // Create Cornell Box
  // Materials
  MaterialHandle red_mat = renderer.add_lambert_material({0.65f, 0.05f, 0.05f});
  MaterialHandle white_mat =
      renderer.add_lambert_material({0.73f, 0.73f, 0.73f});
  MaterialHandle green_mat =
      renderer.add_lambert_material({0.12f, 0.45f, 0.15f});
  MaterialHandle emissive_mat =
      renderer.add_emissive_material({15.f, 15.f, 15.f});

  // FLOOR
  {
    Transform t;
    t.position = glm::vec3(0.0f, 0.0f, -0.025f);
    t.scale = glm::vec3(2.0f, 1.0f, 2.0f);

    renderer.add_blas_instance(renderer.plane_blas_index, t.get_mat4(),
                               white_mat);
  }

  // CEILING
  {
    Transform t;
    t.position = glm::vec3(0.0f, 1.99f, -0.025f);
    t.scale = glm::vec3(2.0f, 1.0f, 2.0f);
    t.rotation = glm::vec4(1, 0, 0, glm::pi<float>());

    renderer.add_blas_instance(renderer.plane_blas_index, t.get_mat4(),
                               white_mat);
  }

  // BACK WALL
  {
    Transform t;
    t.position = glm::vec3(0.0f, 1.0f, -1.0f);
    t.scale = glm::vec3(2.0f, 1.0f, 2.0f);
    t.rotation = glm::vec4(1, 0, 0, -glm::half_pi<float>());

    renderer.add_blas_instance(renderer.plane_blas_index, t.get_mat4(),
                               white_mat);
  }

  // LEFT WALL (RED)
  {
    Transform t;
    t.position = glm::vec3(-1.0f, 1.0f, -0.025f);
    t.scale = glm::vec3(2.0f, 1.0f, 2.0f);
    t.rotation = glm::vec4(0, 0, 1, glm::half_pi<float>());

    renderer.add_blas_instance(renderer.plane_blas_index, t.get_mat4(),
                               red_mat);
  }

  // RIGHT WALL (GREEN)
  {
    Transform t;
    t.position = glm::vec3(1.0f, 1.0f, -0.025f);
    t.scale = glm::vec3(2.0f, 1.0f, 2.0f);
    t.rotation = glm::vec4(0, 0, 1, -glm::half_pi<float>());

    renderer.add_blas_instance(renderer.plane_blas_index, t.get_mat4(),
                               green_mat);
  }

  // LIGHT (small ceiling panel)
  {
    Transform t;
    t.position = glm::vec3(0.0f, 1.98f, -0.03f);
    t.scale = glm::vec3(0.5f, 1.0f, 0.4f);
    t.rotation = glm::vec4(1, 0, 0, glm::pi<float>());

    renderer.add_blas_instance(renderer.plane_blas_index, t.get_mat4(),
                               emissive_mat);
  }
  // short box
  {
    Transform t;
    t.position = glm::vec3(0.3f, 0.3f, 0.35f);
    t.scale = glm::vec3(0.6f, 0.6f, 0.6f);
    t.rotation = glm::vec4(0, 1, 0, glm::radians(-18.f));

    renderer.add_blas_instance(renderer.sphere_blas_index, t.get_mat4(),
                               white_mat);
  }

  // tall box
  {
    Transform t;
    t.position = glm::vec3(-0.4f, 0.6f, -0.3f);
    t.scale = glm::vec3(0.6f, 1.2f, 0.6f);
    t.rotation = glm::vec4(0, 1, 0, glm::radians(15.f));

    renderer.add_blas_instance(renderer.cube_blas_index, t.get_mat4(),
                               white_mat);
  }

  // Testing remove_blas_instance, should remove the floor
  renderer.remove_blas_instance(1);
}

void PathTracer::run() {
  ShaderHandle vert_shader;
  ShaderHandle frag_shader;
  PipelineHandle fullscreen_pipeline;

  // Create the fullscreen pipeline sampler
  VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sampler_info.magFilter = VK_FILTER_NEAREST;
  sampler_info.minFilter = VK_FILTER_NEAREST;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.anisotropyEnable = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.minLod = 0.f;
  sampler_info.maxLod = VK_LOD_CLAMP_NONE;
  sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  fullscreen_sampler = rm.create_sampler("FullscreenSampler", sampler_info);

  // Compile the fullscreen pipeline's shaders
  ShaderBlob blob;
  try {
    SlangCompiler::compile_code("vertMain", "Fullscreen",
                                ASSETS_PATH "/Shaders/Fullscreen.slang", blob);
    vert_shader = rm.create_shader("FullscreenVert", blob);

    SlangCompiler::compile_code("fragMain", "Fullscreen",
                                ASSETS_PATH "/Shaders/Fullscreen.slang", blob);
    frag_shader = rm.create_shader("FullscreenFrag", blob);

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
  SetLayoutHandle final_image_set_layout = rm.create_descriptor_set_layout(
      "FullscreenDescriptorSetLayout", layout_info);

  const VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = device.vk_descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &rm.access_set_layout(final_image_set_layout)->vk_handle};

  VK_CHECK(vkAllocateDescriptorSets(device.vk_device, &alloc_info,
                                    &final_image_set));
  VkDescriptorImageInfo image_info = {
      .sampler = rm.access_sampler(fullscreen_sampler)->vk_handle,
      .imageView = rm.access_image_view(renderer.output_image_view)->vk_handle,
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
          .module = rm.access_shader(vert_shader)->vk_handle,
          .pName = "main",
          .pSpecializationInfo = nullptr},
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = rm.access_shader(frag_shader)->vk_handle,
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
      .pSetLayouts = &rm.access_set_layout(final_image_set_layout)->vk_handle,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant};

  fullscreen_pipeline = rm.create_graphics_pipeline("Fullscreen", pipeline_info,
                                                    pipeline_layout_info);

  Clock clock;
  clock.start();
  f64 last_time = clock.get_elapsed_time_s();
  u64 frame_number = 0;
  Camera cam;
  cam.position = glm::vec3(0.f, 0.75f, 1.5f);
  cam.look_at = glm::vec3(0.f, 0.75f, -1.f);
  cam.fov = 90.f;
  cam.v_up = glm::vec3(0.f, 1.f, 0.f);
  cam.init();
  while (!end_application) {
    Platform::handle_os_messages(scene_ui);
    f64 current_time = clock.get_elapsed_time_s();
    f64 delta_time = current_time - last_time;
    last_time = current_time;

    if (!Platform::is_suspended()) {
      cam.update(delta_time);
      scene_graph.update_transforms();
      device.begin_frame();
      VkCommandBuffer cmd = device.get_current_cmd_buffer();

      renderer.render(cam);

      push_debug_label(cmd, "Fullscreen");
      // Transition the output image to sampled layout
      VulkanImageView *vk_output_image_view =
          rm.access_image_view(renderer.output_image_view);
      VulkanImage *vk_output_image =
          rm.access_image(vk_output_image_view->image_handle);
      VkImageMemoryBarrier2 image_barrier{
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
      image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      image_barrier.subresourceRange.baseMipLevel = 0;
      image_barrier.subresourceRange.levelCount = 1;
      image_barrier.subresourceRange.baseArrayLayer = 0;
      image_barrier.subresourceRange.layerCount = 1;
      image_barrier.image = vk_output_image->vk_handle;
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
      const VkViewport viewport{.x = 0.f,
                                .y = 0.f,
                                .width = static_cast<f32>(screen_extents.width),
                                .height =
                                    static_cast<f32>(screen_extents.height),
                                .minDepth = 0.f,
                                .maxDepth = 1.f};
      vkCmdSetViewport(cmd, 0, 1, &viewport);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        rm.access_pipeline(fullscreen_pipeline)->vk_handle);

      const VkBindDescriptorSetsInfo bind_info{
          .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .layout = rm.access_pipeline(fullscreen_pipeline)->vk_pipeline_layout,
          .firstSet = 0,
          .descriptorSetCount = 1,
          .pDescriptorSets = &final_image_set,
      };
      vkCmdBindDescriptorSets2(cmd, &bind_info);
      f32 total_rays = 0.f;
      vkCmdPushConstants(
          cmd, rm.access_pipeline(fullscreen_pipeline)->vk_pipeline_layout,
          VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(f32), &total_rays);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      pop_debug_label(cmd);

      // Imgui
      scene_ui.begin_frame();
      ImGui::Begin("Scene Graph");
      selected_node_id =
          render_scene_graph_nodes(scene_graph, 0, selected_node_id);
      ImGui::SeparatorText("Scene Node Property");
      render_scene_graph_nodes_property(scene_graph, selected_node_id);
      ImGui::End();
      scene_ui.end_frame();

      vkCmdEndRendering(cmd);
      device.end_frame();
      device.present();
      rm.update(frame_number++);
      Platform::set_title(
          std::format("Path tracer, frame time: {:.3f}s, trig count: {}",
                      delta_time, renderer.total_triangle_count)
              .c_str());
    }
  }

  cam.shutdown();
  rm.queue_destroy({fullscreen_sampler});
  rm.queue_destroy({final_image_set_layout});
  rm.queue_destroy({fullscreen_pipeline});
  rm.queue_destroy({vert_shader});
  rm.queue_destroy({frag_shader});
}

void PathTracer::shutdown() {
  scene_ui.shutdown();
  staging_buffer.shutdown();
  renderer.shutdown();
  SlangCompiler::shutdown();
  rm.shutdown();
  device.shutdown();
  InputSys::shutdown();
  EventSys::unregister_event(SDL_EVENT_WINDOW_RESIZED, this,
                             application_on_resize_event);
  EventSys::unregister_event(SDL_EVENT_QUIT, this, application_on_event);
  EventSys::unregister_event(SDL_EVENT_KEY_DOWN, this, application_on_key);
  EventSys::unregister_event(SDL_EVENT_KEY_UP, this, application_on_key);
  EventSys::shutdown();
  Platform::shutdown();
}

void PathTracer::resize() {
  device.reset();
  renderer.resize(device.back_buffer_width, device.back_buffer_height);

  VkDescriptorImageInfo image_info = {
      .sampler = rm.access_sampler(fullscreen_sampler)->vk_handle,
      .imageView = rm.access_image_view(renderer.output_image_view)->vk_handle,
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
}

} // namespace hlx
