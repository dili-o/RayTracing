#pragma once
#include "Vec3.hpp"

struct AABB {
	AABB() : min(infinity), max(-infinity) {}

	void grow(const Vec3 &p) {
		min = Vec3::fmin(min, p);
		max = Vec3::fmax(max, p);
	}

	void grow(const AABB &b) {
		min = Vec3::fmin(min, b.min);
		max = Vec3::fmax(max, b.max);
	}

	f32 half_area() {
		Vec3 e = max - min;
		return e.x * e.y + e.y * e.z + e.x * e.z;
	}

	Vec3 min;
	Vec3 max;
};
