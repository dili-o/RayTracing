#pragma once

struct Interval {
  float min;
  float max;

  __device__ Interval(float min_, float max_) {
    min = min_;
    max = max_;
  }

  __device__ float size() const { return max - min; }

  __device__ bool contains(float x) const { return min <= x && x <= max; }

  __device__ bool surrounds(float x) const { return min < x && x < max; }

  __device__ float clamp(float x) const {
    if (x < min)
      return min;
    if (x > max)
      return max;
    return x;
  }
};
