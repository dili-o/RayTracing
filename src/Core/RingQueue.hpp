#pragma once

#include "Core/Assert.hpp"

namespace hlx {
// First In First Out
template <typename T> struct RingQueue {

  RingQueue();
  ~RingQueue();

  void init(u32 initial_capacity);
  void shutdown();
  bool enqueue(const T &element);
  bool dequeue(T *out_element);
  bool peek_head(T *out_element);

  T *data{nullptr};
  i32 head{0};
  i32 tail{-1};

  u32 size{0};
  u32 capacity{0};
};

// Implementation
template <typename T> inline RingQueue<T>::RingQueue() {}
template <typename T> inline RingQueue<T>::~RingQueue() {}

template <typename T> inline void RingQueue<T>::init(u32 initial_capacity) {
  HASSERT_MSG(!data,
              "Failed to initalize RingQueue, *data is already initialized");
  size = 0;
  capacity = initial_capacity;
  head = 0;
  tail = -1;

  data = static_cast<T *>(malloc(sizeof(T) * initial_capacity));
}

template <typename T> inline void RingQueue<T>::shutdown() {

  HASSERT_MSG(data, "Failed to shutdown RingQueue, *data is uninitialized");
  free(data);

  size = 0;
  capacity = 0;
  head = 0;
  tail = -1;
  data = nullptr;
}

template <typename T> inline bool RingQueue<T>::enqueue(const T &element) {
  if (size == capacity) {
    HERROR("Attempted to enqueue value into full RingQueue");
    return false;
  }

  tail = (tail + 1) % capacity;

  memcpy(&data[tail], &element, sizeof(T));
  ++size;
  return true;
}

template <typename T> inline bool RingQueue<T>::dequeue(T *out_element) {
  if (size == 0) {
    HERROR("Attempted to dequeue in an empty RingQueue");
    return false;
  }

  memcpy(out_element, &data[head], sizeof(T));
  head = (head + 1) % capacity;
  --size;
  return true;
}

template <typename T> inline bool RingQueue<T>::peek_head(T *out_element) {
  if (size == 0) {
    HERROR("Attempted to peek into an empty RingQueue");
    return false;
  }

  memcpy(out_element, &data[head], sizeof(T));
  return true;
}

} // namespace hlx
