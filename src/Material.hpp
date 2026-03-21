#pragma once

enum MaterialType { MATERIAL_LAMBERT, MATERIAL_METAL, MATERIAL_DIELECTRIC };

struct alignas(16) Lambert {
  f32 albedo[4];
};

struct alignas(16) Metal {
  f32 albedo_fuzz[4];
};

struct Dielectric {
  f32 refraction_index;
};

struct MaterialHandle {
  u32 index;
  MaterialType type;
};
