#pragma once

namespace hlx {

#define hkilo(size) (size * 1024)
#define hmega(size) (size * 1024 * 1024)
#define hgiga(size) (size * 1024 * 1024 * 1024)

struct TlsfAllocator {
public:
  void init(size_t size, size_t alignment);
  void shutdown();

  void *allocate(size_t size, size_t alignment);
  void deallocate(void *pointer);

public:
  size_t allocated_size{0};
  size_t max_size{0};
  void *memory{nullptr};

private:
  void *tlsf_handle{nullptr};
  bool is_aligned{false};
};
} // namespace hlx
