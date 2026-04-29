#pragma once
#include "Core/Clock.hpp"
#include "Vulkan/VkResources.hpp"
// Vendor
#include <glm/mat4x4.hpp>

namespace hlx {

struct VkResourceManager;
struct Camera;

struct alignas(16) SkyConstants {
  glm::mat4 gViewProjMat;

  glm::vec4 gColor;

  glm::vec3 gSunIlluminance;
  i32 gScatteringMaxPathDepth;

  glm::uvec2 gResolution;
  f32 gFrameTimeSec;
  f32 gTimeSec;

  glm::uvec2 gMouseLastDownPos;
  u32 gFrameId;
  u32 gTerrainResolution;

  glm::vec2 RayMarchMinMaxSPP;
  f32 gScreenshotCaptureActive;
  f32 pad;
};

struct alignas(16) SkyAtmosphereConstants {
  //
  // From AtmosphereParameters
  //

  glm::vec3 solar_irradiance;
  f32 sun_angular_radius;

  glm::vec3 absorption_extinction;
  f32 mu_s_min;

  glm::vec3 rayleigh_scattering;
  f32 mie_phase_function_g;

  glm::vec3 mie_scattering;
  f32 bottom_radius;

  glm::vec3 mie_extinction;
  f32 top_radius;

  glm::vec3 mie_absorption;
  f32 pad00;

  glm::vec3 ground_albedo;
  f32 pad0;

  glm::vec4 rayleigh_density[3];
  glm::vec4 mie_density[3];
  glm::vec4 absorption_density[3];

  //
  // Add generated static header constant
  //

  i32 TRANSMITTANCE_TEXTURE_WIDTH;
  i32 TRANSMITTANCE_TEXTURE_HEIGHT;
  i32 IRRADIANCE_TEXTURE_WIDTH;
  i32 IRRADIANCE_TEXTURE_HEIGHT;

  i32 SCATTERING_TEXTURE_R_SIZE;
  i32 SCATTERING_TEXTURE_MU_SIZE;
  i32 SCATTERING_TEXTURE_MU_S_SIZE;
  i32 SCATTERING_TEXTURE_NU_SIZE;

  glm::vec3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
  f32 pad3;

  glm::vec3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
  f32 pad4;

  //
  // Other globals
  //
  glm::mat4 gSkyViewProjMat;
  glm::mat4 gSkyInvViewProjMat;
  glm::mat4 gSkyInvProjMat;
  glm::mat4 gSkyInvViewMat;
  glm::mat4 gShadowmapViewProjMat;

  glm::vec3 camera;
  f32 pad5;

  glm::vec3 sun_direction;
  f32 pad6;

  glm::vec3 view_ray;
  f32 pad7;

  f32 MultipleScatteringFactor;
  f32 MultiScatteringLUTRes;
  f32 pad9;
  f32 pad10;
};

static constexpr u32 TRANSMITTANCE_TEXTURE_WIDTH = 256;
static constexpr u32 TRANSMITTANCE_TEXTURE_HEIGHT = 64;

struct LookUpTablesInfo {
  u32 TRANSMITTANCE_TEXTURE_WIDTH = 256;
  u32 TRANSMITTANCE_TEXTURE_HEIGHT = 64;

  u32 SCATTERING_TEXTURE_R_SIZE = 32;
  u32 SCATTERING_TEXTURE_MU_SIZE = 128;
  u32 SCATTERING_TEXTURE_MU_S_SIZE = 32;
  u32 SCATTERING_TEXTURE_NU_SIZE = 8;

  u32 IRRADIANCE_TEXTURE_WIDTH = 64;
  u32 IRRADIANCE_TEXTURE_HEIGHT = 16;

  // Derived from above
  u32 SCATTERING_TEXTURE_WIDTH = 0xDEADBEEF;
  u32 SCATTERING_TEXTURE_HEIGHT = 0xDEADBEEF;
  u32 SCATTERING_TEXTURE_DEPTH = 0xDEADBEEF;

  void updateDerivedData() {
    SCATTERING_TEXTURE_WIDTH =
        SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE;
    SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE;
    SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_R_SIZE;
  }

  LookUpTablesInfo() { updateDerivedData(); }
};

struct SkyRenderer {
public:
  void init(VkResourceManager *p_rm, SamplerHandle texture_sampler);
  void shutdown(VkResourceManager *p_rm);
  void render(VkResourceManager *p_rm, Camera &camera, u32 frame_index);

public:
  std::array<BufferHandle, MAX_FRAMES_IN_FLIGHT> sky_constant_buffers;
  std::array<BufferHandle, MAX_FRAMES_IN_FLIGHT> sky_atmosphere_buffers;
  ImageViewHandle transmittance_lut_texture;
  ImageViewHandle bluenoise_texture;
  ImageViewHandle dummy_texture;

  SetLayoutHandle sky_cb_set_layout;
  std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> sky_cb_sets;
  SetLayoutHandle sky_textures_set_layout;
  VkDescriptorSet sky_textures_set;
  PipelineHandle transmittance_lut_pipeline;
  Clock timer;
};

} // namespace hlx
