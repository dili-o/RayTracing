#pragma once
// Vendor
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace hlx {
struct Transform {
  glm::vec3 position{0.f};
  glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
  glm::vec3 scale{1.f};

  glm::mat4 get_mat4() {
    return glm::translate(glm::mat4(1.f), position) * glm::toMat4(rotation) *
           glm::scale(glm::mat4(1.f), scale);
  }

  void set_transform(const glm::mat4 &matrix) {
    position = glm::vec3(matrix[3]);

    scale.x = glm::length(glm::vec3(matrix[0]));
    scale.y = glm::length(glm::vec3(matrix[1]));
    scale.z = glm::length(glm::vec3(matrix[2]));

    glm::mat3 rotation_matrix;
    rotation_matrix[0] = glm::vec3(matrix[0]) / scale.x;
    rotation_matrix[1] = glm::vec3(matrix[1]) / scale.y;
    rotation_matrix[2] = glm::vec3(matrix[2]) / scale.z;
    rotation = glm::quat_cast(rotation_matrix);
  }
};
} // namespace hlx
