#include "FreeIndexPool.hpp"
#include "Core/Assert.hpp"

namespace hlx {
void FreeIndexPool::init(u32 pool_capacity) {
  HASSERT_MSG(pool_capacity > 0,
              "FreeIndexPool::init() - pool_capacity must be greater than 0");
  capacity = pool_capacity;

  // Group allocate ( resource size + u32 )
  size_t allocation_size = capacity * sizeof(u32);
  free_indices = static_cast<u32 *>(malloc(allocation_size));
  HASSERT(free_indices);
  std::memset(free_indices, 0, allocation_size);
  free_indices_head = 0;

  for (u32 i = 0; i < capacity; ++i) {
    free_indices[i] = i;
  }

  size = 0;
}

void FreeIndexPool::shutdown() {
  if (free_indices_head != 0) {
    HERROR("FreeIndexPool::shutdown() - Pool has {} resources unfreed!.", size);
  }

  HASSERT(size == 0);
  free(free_indices);
}

u32 FreeIndexPool::obtain_new() {
  // Error: no more indices left!
  HASSERT_MSG(free_indices_head < capacity,
              "FreeIndexPool::obtain_new() - No more indices left!.");
  u32 free_index = free_indices[free_indices_head++];
  ++size;
  return free_index;
}

void FreeIndexPool::release(u32 index) {
  HASSERT_MSG(index < capacity, "FreeIndexPool::release() - Attempting to "
                                "release an index outside the capacity");
  HASSERT(free_indices_head != 0);
  free_indices[--free_indices_head] = index;
  --size;
}

void FreeIndexPool::release_all() {
  free_indices_head = 0;
  size = 0;

  for (u32 i = 0; i < capacity; ++i) {
    free_indices[i] = i;
  }
}
} // namespace hlx
