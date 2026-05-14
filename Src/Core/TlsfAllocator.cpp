#include "TlsfAllocator.hpp"
#include "Assert.hpp"
// Vendor
#include <bit>
#include <cstdlib>
#include <tlsf/tlsf.h>

#define TLSF_ALLOCATOR_STATS

namespace hlx {

struct MemoryStatistics {
  size_t allocated_bytes;
  size_t total_bytes;

  u32 allocation_count;

  void add(size_t a) {
    if (a) {
      allocated_bytes += a;
      ++allocation_count;
    }
  }
};

static char *get_memory_usage_str(size_t size, char *buffer) {
  f32 amount = 0.f;
  char unit[4] = "XiB";

  if (size >= hgiga(1)) {
    unit[0] = 'G';
    amount = (f32)size / (f32)hgiga(1);
  } else if (size >= hmega(1)) {
    unit[0] = 'M';
    amount = (f32)size / (f32)hmega(1);
  } else if (size >= hkilo(1)) {
    unit[0] = 'K';
    amount = (f32)size / (f32)hkilo(1);
  } else {
    unit[0] = 'B';
    unit[1] = ' ';
    unit[2] = ' ';
    amount = (f32)size;
  }

  snprintf(buffer, sizeof(char) * 20, "%.2f %s", amount, unit);
  return buffer;
}

static void exit_walker(void *ptr, size_t size, int used, void *user) {
  MemoryStatistics *stats = (MemoryStatistics *)user;
  stats->add(used ? size : 0);

  if (used)
    HERROR("Found active allocation {}, {}", ptr, size);
}

void TlsfAllocator::init(size_t size, size_t alignment) {
  HASSERT(!memory);
  size_t tlsf_overhead =
      tlsf_size() + tlsf_pool_overhead() + tlsf_block_size_min();
  HASSERT_MSGS(size > tlsf_overhead, "size must be greater than {}",
               tlsf_overhead);
  HASSERT((size % alignment) == 0);
  HASSERT(alignment > 0);
  if (alignment > 1) {
    // Ensure the alignment is a power of 2
    HASSERT(std::has_single_bit(alignment));
#if defined(_MSC_VER)
    memory = _aligned_malloc(size, alignment);
#else
    memory = std::aligned_alloc(alignment, size);
#endif
    is_aligned = true;
  } else {
    memory = std::malloc(size);
    is_aligned = false;
  }

  HASSERT(memory);
  max_size = size;
  allocated_size = 0;

  tlsf_handle = tlsf_create_with_pool(memory, size);
  HASSERT(tlsf_handle);
}

void TlsfAllocator::shutdown() {
  MemoryStatistics stats{0, max_size};
  pool_t pool = tlsf_get_pool(tlsf_handle);
  tlsf_walk_pool(pool, exit_walker, (void *)&stats);

  if (stats.allocated_bytes) {
    char str[20];
    HERROR("TlsfAllocator Shutdown - FAILURE! Allocated memory detected. "
           "Allocated {}, total {}",
           get_memory_usage_str(stats.allocated_bytes, str),
           get_memory_usage_str(stats.total_bytes, str));
  } else {
    HINFO("TlsfAllocator Shutdown");
  }

  HASSERT_MSG(stats.allocated_bytes == 0,
              "Allocations still present. Check your code!");

  tlsf_destroy(tlsf_handle);

  if (memory) {
    if (is_aligned) {
#if defined(_MSC_VER)
      _aligned_free(memory);
#else
      std::free(memory);
#endif
    } else {
      std::free(memory);
    }
  }

  memory = nullptr;
}

void *TlsfAllocator::allocate(size_t size, size_t alignment) {
#if defined(TLSF_ALLOCATOR_STATS)
  void *allocated_memory = alignment == 1
                               ? tlsf_malloc(tlsf_handle, size)
                               : tlsf_memalign(tlsf_handle, alignment, size);
  size_t actual_size = tlsf_block_size(allocated_memory);
  allocated_size += actual_size;

  return allocated_memory;
#else
  return tlsf_malloc(tlsf_handle, size);
#endif // TLSF_ALLOCATOR_STATS
}

void TlsfAllocator::deallocate(void *pointer) {
#if defined(TLSF_ALLOCATOR_STATS)
  size_t actual_size = tlsf_block_size(pointer);
  allocated_size -= actual_size;

  tlsf_free(tlsf_handle, pointer);
#else
  tlsf_free(tlsf_handle, pointer);
#endif
}
} // namespace hlx
