#pragma once

#include "VkResources.hpp"

namespace hlx {

struct VkResourceManager;

struct VkRasterizerStates {
public:
  static VkPipelineRasterizationStateCreateInfo no_cull_info();
  static VkPipelineRasterizationStateCreateInfo front_face_cull_info();
  static VkPipelineRasterizationStateCreateInfo front_face_cull_scissor_info();
  static VkPipelineRasterizationStateCreateInfo back_face_cull_info();
  static VkPipelineRasterizationStateCreateInfo back_face_cull_scissor_info();
  static VkPipelineRasterizationStateCreateInfo no_cull_no_ms_info();
  static VkPipelineRasterizationStateCreateInfo no_cull_scissor_info();
  static VkPipelineRasterizationStateCreateInfo wireframe_info();
};

struct VkDepthStencilStates {
public:
  static VkPipelineDepthStencilStateCreateInfo depth_disabled_info();
  static VkPipelineDepthStencilStateCreateInfo depth_enabled_info();
  static VkPipelineDepthStencilStateCreateInfo reverse_depth_enabled_info();
  static VkPipelineDepthStencilStateCreateInfo depth_write_enabled_info();
  static VkPipelineDepthStencilStateCreateInfo
  reverse_depth_write_enabled_info();
  static VkPipelineDepthStencilStateCreateInfo
  depth_stencil_write_enabled_info();
  static VkPipelineDepthStencilStateCreateInfo stencil_enabled_info();
};

struct VkSamplerStates {
public:
  void init(VkResourceManager *p_manager);
  void shutdown(VkResourceManager *p_manager);

  static VkSamplerCreateInfo linear_info();
  static VkSamplerCreateInfo linear_clamp_info();
  static VkSamplerCreateInfo linear_border_info();
  static VkSamplerCreateInfo point_info();
  static VkSamplerCreateInfo anisotropic_info();
  static VkSamplerCreateInfo shadow_map_info();
  static VkSamplerCreateInfo shadow_map_pcf_info();

public:
  SamplerHandle linear_sampler;
  SamplerHandle linear_clamp_sampler;
  SamplerHandle linear_border_sampler;
  SamplerHandle point_sampler;
  SamplerHandle anisotropic_sampler;
  SamplerHandle shadow_map_sampler;
  SamplerHandle shadow_map_pcf_sampler;
};

} // namespace hlx
