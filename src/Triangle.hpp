#pragma once
// Vendor
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace hlx {
struct alignas(16) TriangleGeom {
  TriangleGeom() = default;
  TriangleGeom(const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2)
      : v0(glm::vec4(v0, 1.f)), v1(glm::vec4(v1, 1.f)), v2(glm::vec4(v2, 1.f)) {
    glm::vec3 edge_1 = v1 - v0;
    glm::vec3 edge_2 = v2 - v0;
    normal = glm::vec4(
        glm::normalize(glm::cross(glm::vec3(edge_1), glm::vec3(edge_2))), 1.f);
  }

  glm::vec4 v0;
  glm::vec4 v1;
  glm::vec4 v2;
  glm::vec4 normal;
};

struct alignas(16) TriangleShading {
  TriangleShading() = default;
  TriangleShading(const glm::vec3 &n0, const glm::vec3 &n1, const glm::vec3 &n2)
      : n0(glm::vec4(n0, 1.f)), n1(glm::vec4(n1, 1.f)), n2(glm::vec4(n2, 1.f)) {
  }

  glm::vec4 n0;
  glm::vec4 n1;
  glm::vec4 n2;
};
} // namespace hlx
