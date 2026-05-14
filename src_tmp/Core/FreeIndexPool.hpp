#pragma once

namespace hlx {
struct FreeIndexPool {
public:
  void init(u32 pool_capacity);
  void shutdown();
  u32 obtain_new();
  void release(u32 index);
  void release_all();

public:
  u32 *free_indices{nullptr};
  u32 free_indices_head{0};
  u32 capacity{0};
  u32 size{0};
};
} // namespace hlx
