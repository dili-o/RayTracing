#pragma once
#include "Assert.hpp"

namespace hlx {

#define MIN_POOL_CAPACITY 4

static const u32 k_invalid_index = 0xffffffff;

template <typename Tag> struct ResourceHandle {
  u32 index{k_invalid_index};
  u32 generation{0};

  using HandleTag = Tag; // useful for static_asserts
};

template <typename Tag>
inline bool is_handle_valid(const ResourceHandle<Tag> handle) {
  return handle.index != k_invalid_index;
}

template <typename T>
concept IsResourceHandle = requires {
  typename T::HandleTag;
} && std::is_base_of_v<ResourceHandle<typename T::HandleTag>, T>;

template <IsResourceHandle THandle, typename TResource> struct ResourcePool {
  void init(u32 pool_capacity);
  void shutdown();
  void grow();

  THandle obtain_new();
  TResource *obtain(THandle handle);
  const TResource *obtain(THandle handle) const;

  void release(THandle handle);
  void release_all();

  u8 *memory = nullptr;
  u32 *free_indices = nullptr;
  u32 *generations = nullptr;

  u32 free_indices_head = 0;
  u32 capacity = MIN_POOL_CAPACITY;
  u32 size = 0;
}; // struct ResourcePool

template <IsResourceHandle THandle, typename TResource>
inline void ResourcePool<THandle, TResource>::init(u32 pool_capacity_) {
  capacity =
      pool_capacity_ < MIN_POOL_CAPACITY ? MIN_POOL_CAPACITY : pool_capacity_;

  // Group allocate ( resource size + u32 )
  size_t allocation_size = capacity * (sizeof(TResource) + sizeof(THandle));
  HASSERT(memory = static_cast<u8 *>(malloc(allocation_size)));
  std::memset(memory, 0, allocation_size);

  // Allocate and add free indices
  free_indices = (u32 *)(memory + capacity * sizeof(TResource));
  free_indices_head = 0;

  generations = free_indices + capacity;

  for (u32 i = 0; i < capacity; ++i) {
    free_indices[i] = i;
    generations[i] = 0; // I know it's already zeroed out.
  }

  size = 0;
}

template <IsResourceHandle THandle, typename TResource>
inline void ResourcePool<THandle, TResource>::shutdown() {
  if (free_indices_head != 0) {
    HERROR("Resource pool has unfreed resources. {} resources unfreed!", size);
  }

  HASSERT(size == 0);
  free(memory);
}

template <IsResourceHandle THandle, typename TResource>
inline void ResourcePool<THandle, TResource>::grow() {
  u32 new_capacity = capacity * 2;

  size_t allocation_size = new_capacity * (sizeof(TResource) + sizeof(THandle));

  u8 *new_memory = static_cast<u8 *>(malloc(allocation_size));
  HASSERT(new_memory);
  std::memset(new_memory, 0, allocation_size);

  u32 *new_free_indices =
      (u32 *)(new_memory + new_capacity * sizeof(TResource));

  u32 *new_generations = new_free_indices + new_capacity;

  // Copy the resources into new memory
  std::memcpy((void *)new_memory, (void *)memory, capacity * sizeof(TResource));
  // Copy the free indices into new memory
  std::memcpy((void *)new_free_indices, (void *)free_indices,
              sizeof(u32) * capacity);
  // Copy the generations into new memory
  std::memcpy((void *)new_generations, (void *)generations,
              sizeof(u32) * capacity);

  for (u32 i = size; i < new_capacity; i++) {
    new_free_indices[i] = i;
    new_generations[i] = 0; // Also zeroed
  }

  free(memory);

  capacity = new_capacity;
  memory = new_memory;
  free_indices = new_free_indices;
  generations = new_generations;
}

template <IsResourceHandle THandle, typename TResource>
inline THandle ResourcePool<THandle, TResource>::obtain_new() {
  if (free_indices_head < capacity) {
    u32 free_index = free_indices[free_indices_head++];
    const THandle free_handle = {free_index, generations[free_index]};

    ++size;
    return free_handle;
  }

  // Error: no more resources left!
  HWARN("No more resources left, creating a larger pool");
  grow();
  return obtain_new();
}

template <IsResourceHandle THandle, typename TResource>
inline TResource *ResourcePool<THandle, TResource>::obtain(THandle handle) {
  if (handle.index != k_invalid_index) {
    if (handle.generation == generations[handle.index]) {
      return (TResource *)&memory[handle.index * sizeof(TResource)];
    } else {
      HWARN("Generation mismatch in Handle");
      return nullptr;
    }
  }
  HERROR("Invalid Handle index!");
  return nullptr;
}

template <IsResourceHandle THandle, typename TResource>
inline const TResource *
ResourcePool<THandle, TResource>::obtain(THandle handle) const {
  if (handle.index != k_invalid_index) {
    if (handle.generation == generations[handle.index]) {
      return &memory[handle.index * sizeof(TResource)];
    } else {
      HWARN("Generation mismatch in Handle");
      return nullptr;
    }
  }
  HERROR("Invalid Handle index!");
  return nullptr;
}

template <IsResourceHandle THandle, typename TResource>
inline void ResourcePool<THandle, TResource>::release(THandle handle) {
  if (handle.index != k_invalid_index &&
      handle.generation == generations[handle.index]) {
    free_indices[--free_indices_head] = handle.index;
    --size;
    ++generations[handle.index];
    return;
  }
  HWARN("Attempting to release invalid index");
}

template <IsResourceHandle THandle, typename TResource>
inline void ResourcePool<THandle, TResource>::release_all() {
  free_indices_head = 0;
  size = 0;

  for (u32 i = 0; i < capacity; ++i) {
    free_indices[i] = i;
  }
}
} // namespace hlx
