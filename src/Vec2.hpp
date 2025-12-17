#pragma once

#include "Defines.hpp"

class Vec2 {
public:
	Vec2() : x(0.f), y(0.f) {}
	Vec2(real x, real y) : x(x), y(y) {}

	union {
		struct {
			real x; 
			real y;
		};

		real e[2];
	};
};

inline bool operator==(const Vec2& v, const Vec2& t) { 
  return (v.x == t.x) &&
				 (v.y == t.y);
}

namespace std {
  template<> struct hash<Vec2> {
    size_t operator()(Vec2 const& v) const noexcept {
      size_t h1 = std::hash<float>{}(v.x);
      size_t h2 = std::hash<float>{}(v.y);
      return h1 ^ (h2 << 1);
    }
  };
}
