#ifndef INTERVAL_H
#define INTERVAL_H

#include "Defines.hpp"

class Interval {
public:
  real min, max;

  Interval() : min(+infinity), max(-infinity) {} // Default Interval is empty

  Interval(real min, real max) : min(min), max(max) {}

  real size() const { return max - min; }

  bool contains(real x) const { return min <= x && x <= max; }

  bool surrounds(real x) const { return min < x && x < max; }

  real clamp(real x) const {
    if (x < min)
      return min;
    if (x > max)
      return max;
    return x;
  }

  static const Interval empty, universe;
};

const Interval Interval::empty = Interval(+infinity, -infinity);
const Interval Interval::universe = Interval(-infinity, +infinity);

#endif
