#include "BVHNode.hpp"

namespace hlx {

void BLASInstance::set_transform(const glm::mat4 &transform) {
  this->transform = transform;
  this->inv_transform = glm::inverse(transform);
}

} // namespace hlx
