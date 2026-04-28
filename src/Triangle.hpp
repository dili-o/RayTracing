#pragma once
// Vendor
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace hlx {
struct alignas(16) TriangleGeom {
  TriangleGeom() = default;
  TriangleGeom(const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2)
      : v0(glm::vec4(v0, 1.f)), v1(glm::vec4(v1, 1.f)), v2(glm::vec4(v2, 1.f)) {
  }

  glm::vec4 v0;
  glm::vec4 v1;
  glm::vec4 v2;
};

struct alignas(16) TriangleShading {
  TriangleShading() = default;
  TriangleShading(const glm::vec3 &n0, const glm::vec3 &n1, const glm::vec3 &n2,
                  const glm::vec2 &uv0, const glm::vec2 &uv1,
                  const glm::vec2 &uv2)
      : n0(glm::vec4(n0, 1.f)), n1(glm::vec4(n1, 1.f)), n2(glm::vec4(n2, 1.f)),
        uv0(uv0), uv1(uv1), uv2(uv2) {}

  glm::vec4 n0;
  glm::vec4 n1;
  glm::vec4 n2;
  glm::vec2 uv0;
  glm::vec2 uv1;
  glm::vec2 uv2;
  glm::vec2 padding;
};
} // namespace hlx
