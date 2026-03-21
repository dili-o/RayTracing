#include "SceneUI.hpp"
#include "Core/Exceptions.hpp"
#include "Platform/Platform.hpp"
#include "Vulkan/VkDeviceManager.h"
#include "Vulkan/VkResourceManager.hpp"
#include "Vulkan/VkShaderCompilation.h"
#include "Vulkan/VkStagingBuffer.h"
#include "Vulkan/VkUtils.hpp"
// Vendor
#include <SDL3/SDL_events.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_internal.h>

static constexpr u32 max_vb_size = 665536, max_ib_size = 665536;

namespace hlx {
void SceneUI::init(VkDeviceManager *p_device, VkResourceManager *p_rm,
                   VkStagingBuffer &staging_buffer) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGuiIO &io = ImGui::GetIO();
  io.BackendRendererName = "Helix";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
  HASSERT(ImGui_ImplSDL3_InitForVulkan(
      (SDL_Window *)Platform::get_platform_handle()));

  // Create font texture atlas
  u8 *pixels;
  i32 width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
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
  view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  font_texture_view = p_rm->create_image_view(
      "FontTextureView", "FontTexture", image_info, vma_alloc_info, view_info);

  const size_t image_size = width * height * 4;
  staging_buffer.stage(pixels, font_texture_view, image_size);

  // Create vertex and index buffers
  VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer_info.size = max_vb_size;
  buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  vma_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  i32 index = 0;
  for (auto &vb : vertex_buffers) {
    std::string name = "ImguiVertexBuffer_" + std::to_string(index++);
    vb = p_rm->create_buffer(name, buffer_info, vma_alloc_info);
  }
  index = 0;
  buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  for (auto &ib : index_buffers) {
    std::string name = "ImguiIndexBuffer_" + std::to_string(index++);
    ib = p_rm->create_buffer(name, buffer_info, vma_alloc_info);
  }

  // Load Shaders
  ShaderHandle vert_shader;
  ShaderHandle frag_shader;
  try {
    ShaderBlob blob;
    SlangCompiler::compile_code("vert_main", "Imgui", SHADER_PATH "Imgui.slang",
                                blob);
    vert_shader = p_rm->create_shader("ImguiVertexShader", blob);
    SlangCompiler::compile_code("frag_main", "Imgui", SHADER_PATH "Imgui.slang",
                                blob);
    frag_shader = p_rm->create_shader("ImguiFragmentShader", blob);
  } catch (Exception exception) {
    HERROR("{}", exception.what());
  }

  // Create Descriptor Set Layout
  const std::array<VkDescriptorSetLayoutBinding, 1> set_layout_bindings = {
      VkDescriptorSetLayoutBinding{
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .pImmutableSamplers = nullptr}};

  VkDescriptorSetLayoutCreateInfo setLayoutInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  setLayoutInfo.bindingCount = set_layout_bindings.size();
  setLayoutInfo.pBindings = set_layout_bindings.data();

  set_layout =
      p_rm->create_descriptor_set_layout("ImguiSetLayout", setLayoutInfo);
  VulkanSetLayout *vk_set_layout = p_rm->access_set_layout(set_layout);
  // Allocate dset
  VkDescriptorSetAllocateInfo set_alloc_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  set_alloc_info.descriptorPool = p_device->vk_descriptor_pool;
  set_alloc_info.descriptorSetCount = 1;
  set_alloc_info.pSetLayouts = &vk_set_layout->vk_handle;
  VK_CHECK(
      vkAllocateDescriptorSets(p_device->vk_device, &set_alloc_info, &set));
  p_device->set_resource_name<VkDescriptorSet>(VK_OBJECT_TYPE_DESCRIPTOR_SET,
                                               set, "ImguiSet");
  // Create Sampler
  VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.mipLodBias = 0;
  sampler_info.compareEnable = VK_TRUE;
  sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_info.minLod = 0;
  sampler_info.maxLod = VK_LOD_CLAMP_NONE;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  linear_sampler = p_rm->create_sampler("ImguiSampler", sampler_info);

  // Update descriptor set
  const VkDescriptorImageInfo image_write_info{
      .sampler = p_rm->access_sampler(linear_sampler)->vk_handle,
      .imageView = p_rm->access_image_view(font_texture_view)->vk_handle,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  const VkWriteDescriptorSet descriptorWrite{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = nullptr,
      .dstSet = set,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &image_write_info,
      .pBufferInfo = nullptr,
      .pTexelBufferView = nullptr};
  vkUpdateDescriptorSets(p_device->vk_device, 1, &descriptorWrite, 0, nullptr);

  // Create pipeline
  const VkPushConstantRange push_constant{.stageFlags =
                                              VK_SHADER_STAGE_VERTEX_BIT,
                                          .offset = 0,
                                          .size = sizeof(f32) * 4};
  VkPipelineLayoutCreateInfo pipeline_layout_info{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &vk_set_layout->vk_handle;
  pipeline_layout_info.pushConstantRangeCount = 1;
  pipeline_layout_info.pPushConstantRanges = &push_constant;

  VkVertexInputBindingDescription vertex_input_binding{};
  vertex_input_binding.binding = 0;
  vertex_input_binding.stride = sizeof(ImDrawVert);
  vertex_input_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  const std::array<VkVertexInputAttributeDescription, 3>
      vertex_input_attributes = {
          VkVertexInputAttributeDescription{.location = 0,
                                            .binding = 0,
                                            .format = VK_FORMAT_R32G32_SFLOAT,
                                            .offset =
                                                offsetof(ImDrawVert, pos)},
          VkVertexInputAttributeDescription{.location = 1,
                                            .binding = 0,
                                            .format = VK_FORMAT_R32G32_SFLOAT,
                                            .offset = offsetof(ImDrawVert, uv)},
          VkVertexInputAttributeDescription{.location = 2,
                                            .binding = 0,
                                            .format = VK_FORMAT_R8G8B8A8_UNORM,
                                            .offset =
                                                offsetof(ImDrawVert, col)},
      };

  VkPipelineVertexInputStateCreateInfo vertex_input_info{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vertex_input_info.vertexBindingDescriptionCount = 1;
  vertex_input_info.pVertexBindingDescriptions = &vertex_input_binding;
  vertex_input_info.vertexAttributeDescriptionCount =
      vertex_input_attributes.size();
  vertex_input_info.pVertexAttributeDescriptions =
      vertex_input_attributes.data();

  std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = p_rm->access_shader(vert_shader)->vk_handle,
          .pName = "main",
          .pSpecializationInfo = nullptr},
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = p_rm->access_shader(frag_shader)->vk_handle,
          .pName = "main",
          .pSpecializationInfo = nullptr},
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewport_info{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewport_info.viewportCount = 1;
  viewport_info.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo raster_state{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  raster_state.depthClampEnable = VK_FALSE;
  raster_state.rasterizerDiscardEnable = VK_FALSE;
  raster_state.polygonMode = VK_POLYGON_MODE_FILL;
  raster_state.lineWidth = 1.0f;
  raster_state.cullMode = VK_CULL_MODE_NONE;
  raster_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
  raster_state.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling_state{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisampling_state.sampleShadingEnable = VK_FALSE;
  multisampling_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  depth_stencil_state.depthTestEnable = VK_FALSE;
  depth_stencil_state.depthWriteEnable = VK_FALSE;
  depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
  depth_stencil_state.minDepthBounds = 0.0f;
  depth_stencil_state.maxDepthBounds = 1.0f;
  depth_stencil_state.stencilTestEnable = VK_FALSE;

  const VkPipelineColorBlendAttachmentState color_blend_attachment{
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

  VkPipelineColorBlendStateCreateInfo color_blend_state{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  color_blend_state.logicOpEnable = VK_FALSE;
  color_blend_state.logicOp = VK_LOGIC_OP_CLEAR;
  color_blend_state.attachmentCount = 1;
  color_blend_state.pAttachments = &color_blend_attachment;

  std::array<VkDynamicState, 2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamic_state{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic_state.dynamicStateCount = dynamic_states.size();
  dynamic_state.pDynamicStates = dynamic_states.data();

  const VkFormat colorFormat = p_device->swapchain.vk_surface_format.format;
  VkPipelineRenderingCreateInfo rendering_info{
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
  rendering_info.colorAttachmentCount = 1;
  rendering_info.pColorAttachmentFormats = &colorFormat;
  rendering_info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
  rendering_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

  VkGraphicsPipelineCreateInfo pipeline_info{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pipeline_info.pNext = &rendering_info;
  pipeline_info.stageCount = shader_stages.size();
  pipeline_info.pStages = shader_stages.data();
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly_info;
  pipeline_info.pViewportState = &viewport_info;
  pipeline_info.pRasterizationState = &raster_state;
  pipeline_info.pMultisampleState = &multisampling_state;
  pipeline_info.pDepthStencilState = &depth_stencil_state;
  pipeline_info.pColorBlendState = &color_blend_state;
  pipeline_info.pDynamicState = &dynamic_state;

  pipeline = p_rm->create_graphics_pipeline("ImguiPipeline", pipeline_info,
                                            pipeline_layout_info);

  p_rm->queue_destroy({vert_shader});
  p_rm->queue_destroy({frag_shader});
  this->p_device = p_device;
  this->p_rm = p_rm;
}

void SceneUI::shutdown() {
  p_rm->queue_destroy({pipeline});
  p_rm->queue_destroy({set_layout});
  p_rm->queue_destroy({linear_sampler});
  p_rm->queue_destroy({font_texture_view});
  for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    p_rm->queue_destroy({vertex_buffers[i]});
    p_rm->queue_destroy({index_buffers[i]});
  }
  ImGui_ImplSDL3_Shutdown();
}

bool SceneUI::handle_events(void *event_) {
  SDL_Event *event = (SDL_Event *)event_;
  ImGui_ImplSDL3_ProcessEvent(event);
  if (event->type == SDL_EVENT_QUIT || event->type == SDL_EVENT_WINDOW_RESIZED)
    return false;
  return ImGui::GetCurrentContext()->NavWindow;
}

void SceneUI::begin_frame() {
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
}

void SceneUI::end_frame() {
  ImGui::Render();

  ImDrawData *draw_data = ImGui::GetDrawData();

  // Avoid rendering when minimized, scale coordinates for retina displays
  // (screen coordinates != framebuffer coordinates)
  i32 fb_width =
      (i32)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
  i32 fb_height =
      (i32)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
  if (fb_width <= 0 || fb_height <= 0)
    return;

  size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
  size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

  if (vertex_size >= max_vb_size || index_size >= max_ib_size) {
    HERROR("ImGui Backend Error: vertex/index overflow!");
    return;
  }

  if (vertex_size == 0 && index_size == 0) {
    return;
  }

  // NOTE: Command buffer should already be recording
  VkCommandBuffer cmd = p_device->get_current_cmd_buffer();
  push_debug_label(cmd, "Imgui");
  // Upload vertex and index data
  VulkanBuffer *vertex_buffer =
      p_rm->access_buffer(vertex_buffers[p_device->current_frame]);
  VulkanBuffer *index_buffer =
      p_rm->access_buffer(index_buffers[p_device->current_frame]);

  ImDrawVert *vtx_dst = (ImDrawVert *)vertex_buffer->p_data;
  if (vtx_dst) {
    for (i32 n = 0; n < draw_data->CmdListsCount; n++) {
      const ImDrawList *cmd_list = draw_data->CmdLists[n];
      memcpy(vtx_dst, cmd_list->VtxBuffer.Data,
             cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
      vtx_dst += cmd_list->VtxBuffer.Size;
    }
  }

  ImDrawIdx *idx_dst = (ImDrawIdx *)index_buffer->p_data;
  if (idx_dst) {
    for (i32 n = 0; n < draw_data->CmdListsCount; n++) {
      const ImDrawList *cmd_list = draw_data->CmdLists[n];
      memcpy(idx_dst, cmd_list->IdxBuffer.Data,
             cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
      idx_dst += cmd_list->IdxBuffer.Size;
    }
  }

  VulkanPipeline *vk_pipeline = p_rm->access_pipeline(pipeline);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    vk_pipeline->vk_handle);

  VkDeviceSize offset{0};
  vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer->vk_handle, &offset);
  vkCmdBindIndexBuffer(cmd, index_buffer->vk_handle, 0, VK_INDEX_TYPE_UINT16);
  const VkViewport viewport{
      .x = 0.f,
      .y = 0.f,
      .width = static_cast<f32>(p_device->back_buffer_width),
      .height = static_cast<f32>(p_device->back_buffer_height),
      .minDepth = 0.f,
      .maxDepth = 1.f};
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  // Bind descriptors
  VkBindDescriptorSetsInfo bind_info{
      VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO};
  bind_info.pNext = nullptr;
  bind_info.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  bind_info.layout = vk_pipeline->vk_pipeline_layout;
  bind_info.firstSet = 0;
  bind_info.descriptorSetCount = 1;
  bind_info.pDescriptorSets = &set;
  bind_info.dynamicOffsetCount = 0;
  bind_info.pDynamicOffsets = nullptr;
  vkCmdBindDescriptorSets2(cmd, &bind_info);

  // Setup push constants
  f32 scale[2];
  scale[0] = 2.0f / draw_data->DisplaySize.x;
  scale[1] = 2.0f / draw_data->DisplaySize.y;
  f32 translate[2];
  translate[0] = -1.0f - draw_data->DisplayPos.x * scale[0];
  translate[1] = -1.0f - draw_data->DisplayPos.y * scale[1];

  f32 uniform[4];
  uniform[0] = scale[0];
  uniform[1] = scale[1];
  uniform[2] = translate[0];
  uniform[3] = translate[1];

  vkCmdPushConstants(cmd, vk_pipeline->vk_pipeline_layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(f32) * 4, &uniform);

  // Will project scissor/clipping rectangles into framebuffer space
  ImVec2 clip_off = draw_data->DisplayPos; // (0,0) unless using multi-viewports
  ImVec2 clip_scale =
      draw_data->FramebufferScale; // (1,1) unless using retina display which
                                   // are often (2,2)

  // Render command lists
  i32 counts = draw_data->CmdListsCount;

  u32 vtx_buffer_offset = 0, index_buffer_offset = 0;
  for (i32 n = 0; n < counts; n++) {
    const ImDrawList *cmd_list = draw_data->CmdLists[n];

    for (i32 cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
      const ImDrawCmd *p_cmd = &cmd_list->CmdBuffer[cmd_i];
      if (p_cmd->UserCallback) {
        // User callback (registered via ImDrawList::AddCallback)
        p_cmd->UserCallback(cmd_list, p_cmd);
      } else {
        // Project scissor/clipping rectangles into framebuffer space
        ImVec4 clip_rect;
        clip_rect.x = (p_cmd->ClipRect.x - clip_off.x) * clip_scale.x;
        clip_rect.y = (p_cmd->ClipRect.y - clip_off.y) * clip_scale.y;
        clip_rect.z = (p_cmd->ClipRect.z - clip_off.x) * clip_scale.x;
        clip_rect.w = (p_cmd->ClipRect.w - clip_off.y) * clip_scale.y;

        if (clip_rect.x < fb_width && clip_rect.y < fb_height &&
            clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
          // Apply scissor/clipping rectangle
          f32 offsets[2] = {clip_rect.x, clip_rect.y};
          f32 extents[2] = {(clip_rect.z - clip_rect.x),
                            (clip_rect.w - clip_rect.y)};
          const VkRect2D scissor{.offset = {static_cast<i32>(offsets[0]),
                                            static_cast<i32>(offsets[1])},
                                 .extent = {static_cast<u32>(extents[0]),
                                            static_cast<u32>(extents[1])}};
          vkCmdSetScissor(cmd, 0, 1, &scissor);

          // Draw GUI
          vkCmdDrawIndexed(cmd, p_cmd->ElemCount, 1,
                           index_buffer_offset + p_cmd->IdxOffset,
                           vtx_buffer_offset + p_cmd->VtxOffset, 0);
        }
      }
    }
    index_buffer_offset += cmd_list->IdxBuffer.Size;
    vtx_buffer_offset += cmd_list->VtxBuffer.Size;
  }
  pop_debug_label(cmd);
}
} // namespace hlx
