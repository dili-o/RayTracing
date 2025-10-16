#pragma once

struct Interval {
  float min;
  float max;

  __init(float min_, float max_) {
    min = min_;
    max = max_;
  }

  float size() {
    return max - min;
  }

  bool contains(float x) {
    return min <= x && x <= max;
  }

  bool surrounds(float x) {
    return min < x && x < max;
  }

  float clamp(float x) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
  }
};





