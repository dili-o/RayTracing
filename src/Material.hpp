#pragma once

enum MaterialType {
  MATERIAL_LAMBERT,
  MATERIAL_METAL,
  MATERIAL_DIELECTRIC,
  MATERIAL_EMISSIVE
};

struct alignas(16) Lambert {
  f32 albedo[4];
};

struct alignas(16) Metal {
  f32 albedo_fuzz[4];
};

struct Dielectric {
  f32 refraction_index;
};

struct Emissive {
  f32 intensity[4];
};

struct MaterialHandle {
  u32 index;
  MaterialType type;
};
