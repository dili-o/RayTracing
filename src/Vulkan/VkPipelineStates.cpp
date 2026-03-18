#include "VkPipelineStates.hpp"

namespace hlx {

VkPipelineRasterizationStateCreateInfo VkRasterizerStates::no_cull_info() {
  VkPipelineRasterizationStateCreateInfo rast_info{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rast_info.depthClampEnable = VK_FALSE;
  rast_info.rasterizerDiscardEnable = VK_FALSE;
  rast_info.polygonMode = VK_POLYGON_MODE_FILL;
  rast_info.cullMode = VK_CULL_MODE_NONE;
  rast_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rast_info.depthBiasEnable = VK_FALSE;
  rast_info.depthBiasConstantFactor = 0.0f;
  rast_info.depthBiasClamp = 0.0f;
  rast_info.depthBiasSlopeFactor = 0.0f;
  rast_info.lineWidth = 1.0f;
  return rast_info;
}

VkPipelineRasterizationStateCreateInfo
VkRasterizerStates::front_face_cull_info() {
  VkPipelineRasterizationStateCreateInfo rast_info{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rast_info.depthClampEnable = VK_FALSE;
  rast_info.rasterizerDiscardEnable = VK_FALSE;
  rast_info.polygonMode = VK_POLYGON_MODE_FILL;
  rast_info.cullMode = VK_CULL_MODE_NONE;
  rast_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rast_info.depthBiasEnable = VK_FALSE;
  rast_info.depthBiasConstantFactor = 0.0f;
  rast_info.depthBiasClamp = 0.0f;
  rast_info.depthBiasSlopeFactor = 0.0f;
  rast_info.lineWidth = 1.0f;
  return rast_info;
}

VkPipelineRasterizationStateCreateInfo
VkRasterizerStates::front_face_cull_scissor_info() {
  VkPipelineRasterizationStateCreateInfo rast_info{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rast_info.depthClampEnable = VK_FALSE;
  rast_info.rasterizerDiscardEnable = VK_FALSE;
  rast_info.polygonMode = VK_POLYGON_MODE_FILL;
  rast_info.cullMode = VK_CULL_MODE_NONE;
  rast_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rast_info.depthBiasEnable = VK_FALSE;
  rast_info.depthBiasConstantFactor = 0.0f;
  rast_info.depthBiasClamp = 0.0f;
  rast_info.depthBiasSlopeFactor = 0.0f;
  rast_info.lineWidth = 1.0f;
  return rast_info;
}

VkPipelineRasterizationStateCreateInfo
VkRasterizerStates::back_face_cull_info() {
  VkPipelineRasterizationStateCreateInfo rast_info{};
  rast_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

  rast_info.depthClampEnable = VK_FALSE;
  rast_info.rasterizerDiscardEnable = VK_FALSE;

  rast_info.polygonMode = VK_POLYGON_MODE_FILL;
  rast_info.cullMode = VK_CULL_MODE_BACK_BIT;
  rast_info.frontFace = VK_FRONT_FACE_CLOCKWISE;

  rast_info.depthBiasEnable = VK_FALSE;
  rast_info.lineWidth = 1.0f;
  return rast_info;
}

VkPipelineRasterizationStateCreateInfo
VkRasterizerStates::back_face_cull_scissor_info() {
  VkPipelineRasterizationStateCreateInfo rast_info{};
  rast_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

  rast_info.depthClampEnable = VK_FALSE;
  rast_info.rasterizerDiscardEnable = VK_FALSE;

  rast_info.polygonMode = VK_POLYGON_MODE_FILL;
  rast_info.cullMode = VK_CULL_MODE_BACK_BIT;
  rast_info.frontFace = VK_FRONT_FACE_CLOCKWISE;

  rast_info.depthBiasEnable = VK_FALSE;
  rast_info.lineWidth = 1.0f;
  return rast_info;
}

VkPipelineRasterizationStateCreateInfo
VkRasterizerStates::no_cull_no_ms_info() {
  VkPipelineRasterizationStateCreateInfo rast_info{};
  rast_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

  rast_info.depthClampEnable = VK_FALSE;
  rast_info.rasterizerDiscardEnable = VK_FALSE;

  rast_info.polygonMode = VK_POLYGON_MODE_FILL;
  rast_info.cullMode = VK_CULL_MODE_NONE;
  rast_info.frontFace = VK_FRONT_FACE_CLOCKWISE;

  rast_info.depthBiasEnable = VK_FALSE;
  rast_info.lineWidth = 1.0f;
  return rast_info;
}

VkPipelineRasterizationStateCreateInfo
VkRasterizerStates::no_cull_scissor_info() {
  VkPipelineRasterizationStateCreateInfo rast_info{};
  rast_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

  rast_info.depthClampEnable = VK_FALSE;
  rast_info.rasterizerDiscardEnable = VK_FALSE;

  rast_info.polygonMode = VK_POLYGON_MODE_FILL;
  rast_info.cullMode = VK_CULL_MODE_NONE;
  rast_info.frontFace = VK_FRONT_FACE_CLOCKWISE;

  rast_info.depthBiasEnable = VK_FALSE;
  rast_info.lineWidth = 1.0f;
  return rast_info;
}

VkPipelineRasterizationStateCreateInfo VkRasterizerStates::wireframe_info() {
  VkPipelineRasterizationStateCreateInfo rast_info{};
  rast_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

  rast_info.depthClampEnable = VK_FALSE;
  rast_info.rasterizerDiscardEnable = VK_FALSE;

  rast_info.polygonMode = VK_POLYGON_MODE_LINE;
  rast_info.cullMode = VK_CULL_MODE_NONE;
  rast_info.frontFace = VK_FRONT_FACE_CLOCKWISE;

  rast_info.depthBiasEnable = VK_FALSE;
  rast_info.lineWidth = 1.0f;
  return rast_info;
}

// VkDepthStencilStates
VkPipelineDepthStencilStateCreateInfo
VkDepthStencilStates::depth_disabled_info() {
  VkPipelineDepthStencilStateCreateInfo ds_info{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds_info.depthTestEnable = VK_FALSE;
  ds_info.depthWriteEnable = VK_FALSE;
  ds_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  ds_info.stencilTestEnable = VK_FALSE;
  ds_info.front.failOp = VK_STENCIL_OP_KEEP;
  ds_info.front.passOp = VK_STENCIL_OP_REPLACE;
  ds_info.front.depthFailOp = VK_STENCIL_OP_KEEP;
  ds_info.front.compareOp = VK_COMPARE_OP_ALWAYS;
  ds_info.front.compareMask = 0xFF;
  ds_info.front.writeMask = 0xFF;
  ds_info.back = ds_info.front;
  ds_info.depthBoundsTestEnable = VK_FALSE;
  ds_info.minDepthBounds = 0.0f;
  ds_info.maxDepthBounds = 1.0f;
  return ds_info;
}

VkPipelineDepthStencilStateCreateInfo
VkDepthStencilStates::depth_enabled_info() {
  VkPipelineDepthStencilStateCreateInfo ds_info{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds_info.depthTestEnable = VK_TRUE;
  ds_info.depthWriteEnable = VK_FALSE;
  ds_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  ds_info.stencilTestEnable = VK_FALSE;
  ds_info.front.failOp = VK_STENCIL_OP_KEEP;
  ds_info.front.passOp = VK_STENCIL_OP_REPLACE;
  ds_info.front.depthFailOp = VK_STENCIL_OP_KEEP;
  ds_info.front.compareOp = VK_COMPARE_OP_ALWAYS;
  ds_info.front.compareMask = 0xFF;
  ds_info.front.writeMask = 0xFF;
  ds_info.back = ds_info.front;
  ds_info.minDepthBounds = 0.0f;
  ds_info.maxDepthBounds = 1.0f;
  return ds_info;
}

VkPipelineDepthStencilStateCreateInfo
VkDepthStencilStates::reverse_depth_enabled_info() {
  VkPipelineDepthStencilStateCreateInfo ds_info{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds_info.depthTestEnable = VK_TRUE;
  ds_info.depthWriteEnable = VK_FALSE;
  ds_info.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
  ds_info.stencilTestEnable = VK_FALSE;
  ds_info.front.failOp = VK_STENCIL_OP_KEEP;
  ds_info.front.passOp = VK_STENCIL_OP_REPLACE;
  ds_info.front.depthFailOp = VK_STENCIL_OP_KEEP;
  ds_info.front.compareOp = VK_COMPARE_OP_ALWAYS;
  ds_info.front.compareMask = 0xFF;
  ds_info.front.writeMask = 0xFF;
  ds_info.back = ds_info.front;
  ds_info.minDepthBounds = 0.0f;
  ds_info.maxDepthBounds = 1.0f;
  return ds_info;
}

VkPipelineDepthStencilStateCreateInfo
VkDepthStencilStates::depth_write_enabled_info() {
  VkPipelineDepthStencilStateCreateInfo ds_info{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds_info.depthTestEnable = VK_TRUE;
  ds_info.depthWriteEnable = VK_TRUE;
  ds_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  ds_info.depthBoundsTestEnable = VK_FALSE;
  ds_info.stencilTestEnable = VK_FALSE;
  ds_info.front.failOp = VK_STENCIL_OP_KEEP;
  ds_info.front.passOp = VK_STENCIL_OP_REPLACE;
  ds_info.front.depthFailOp = VK_STENCIL_OP_KEEP;
  ds_info.front.compareOp = VK_COMPARE_OP_ALWAYS;
  ds_info.front.compareMask = 0xFF;
  ds_info.front.writeMask = 0xFF;
  ds_info.front.reference = 0;
  ds_info.back = ds_info.front;
  ds_info.minDepthBounds = 0.0f;
  ds_info.maxDepthBounds = 1.0f;

  return ds_info;
}

VkPipelineDepthStencilStateCreateInfo
VkDepthStencilStates::reverse_depth_write_enabled_info() {
  VkPipelineDepthStencilStateCreateInfo ds_info{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds_info.depthTestEnable = VK_TRUE;
  ds_info.depthWriteEnable = VK_TRUE;
  ds_info.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
  ds_info.depthBoundsTestEnable = VK_FALSE;
  ds_info.stencilTestEnable = VK_FALSE;
  ds_info.front.failOp = VK_STENCIL_OP_KEEP;
  ds_info.front.passOp = VK_STENCIL_OP_REPLACE;
  ds_info.front.depthFailOp = VK_STENCIL_OP_KEEP;
  ds_info.front.compareOp = VK_COMPARE_OP_ALWAYS;
  ds_info.front.compareMask = 0xFF;
  ds_info.front.writeMask = 0xFF;
  ds_info.front.reference = 0;
  ds_info.back = ds_info.front;
  ds_info.minDepthBounds = 0.0f;
  ds_info.maxDepthBounds = 1.0f;

  return ds_info;
}

VkPipelineDepthStencilStateCreateInfo
VkDepthStencilStates::depth_stencil_write_enabled_info() {
  VkPipelineDepthStencilStateCreateInfo ds_info{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds_info.depthTestEnable = VK_TRUE;
  ds_info.depthWriteEnable = VK_TRUE;
  ds_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  ds_info.depthBoundsTestEnable = VK_FALSE;
  ds_info.stencilTestEnable = VK_TRUE;
  ds_info.front.failOp = VK_STENCIL_OP_KEEP;
  ds_info.front.passOp = VK_STENCIL_OP_REPLACE;
  ds_info.front.depthFailOp = VK_STENCIL_OP_KEEP;
  ds_info.front.compareOp = VK_COMPARE_OP_ALWAYS;
  ds_info.front.compareMask = 0xFF;
  ds_info.front.writeMask = 0xFF;
  ds_info.front.reference = 0;
  ds_info.back = ds_info.front;
  ds_info.minDepthBounds = 0.0f;
  ds_info.maxDepthBounds = 1.0f;

  return ds_info;
}

VkPipelineDepthStencilStateCreateInfo
VkDepthStencilStates::stencil_enabled_info() {
  VkPipelineDepthStencilStateCreateInfo ds_info{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds_info.depthTestEnable = VK_TRUE;
  ds_info.depthWriteEnable = VK_TRUE;
  ds_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  ds_info.depthBoundsTestEnable = VK_FALSE;
  ds_info.stencilTestEnable = VK_TRUE;
  ds_info.front.failOp = VK_STENCIL_OP_KEEP;
  ds_info.front.passOp = VK_STENCIL_OP_KEEP;
  ds_info.front.depthFailOp = VK_STENCIL_OP_KEEP;
  ds_info.front.compareOp = VK_COMPARE_OP_EQUAL;
  ds_info.front.compareMask = 0xFF;
  ds_info.front.writeMask = 0x00;
  ds_info.front.reference = 0;
  ds_info.back = ds_info.front;
  ds_info.minDepthBounds = 0.0f;
  ds_info.maxDepthBounds = 1.0f;

  return ds_info;
}
} // namespace hlx
