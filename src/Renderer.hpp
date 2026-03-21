#pragma once
#include "Camera.hpp"
#include "Vulkan/VkResources.hpp"
// Vendor
#include <glm/fwd.hpp>

namespace hlx {
struct VkDeviceManager;
struct VkResourceManager;
struct VkStagingBuffer;

struct Renderer {
public:
  void init(VkDeviceManager *p_device, VkResourceManager *p_rm,
            VkStagingBuffer &staging_buffer, u32 output_image_width,
            u32 output_image_height);
  void shutdown();
  void resize(u32 output_image_width, u32 output_image_height);
  void render(Camera &camera);
  void create_output_image(u32 width, u32 height);

public:
  VkDeviceManager *p_device{nullptr};
  VkResourceManager *p_rm{nullptr};
  ImageViewHandle output_image_view;
  PipelineHandle path_tracing_pipeline;
  SetLayoutHandle set_layout;
  VkDescriptorSet vk_set;
  std::array<BufferHandle, MAX_FRAMES_IN_FLIGHT> uniform_buffers;
  BufferHandle triangle_geom_buffer;
  BufferHandle triangle_shading_buffer;
  BufferHandle triangle_mat_ids_buffer;
  BufferHandle lambert_materials_buffer;
  BufferHandle metal_materials_buffer;
  BufferHandle dielectric_materials_buffer;
  BufferHandle emissive_materials_buffer;
  u32 triangle_count{0};
  u32 frame_index{0};
};
} // namespace hlx
