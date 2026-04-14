#pragma once

enum MaterialType { LAMBERT, METAL, DIELECTRIC, EMISSIVE, NONE };

struct Lambert {
  u32 index;
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
