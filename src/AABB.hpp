#pragma once
#include "Vec3.hpp"

struct AABB {
	AABB() : min(infinity), max(-infinity) {}

	void grow(Vec3 p) {
		min = Vec3::fmin(min, p);
		max = Vec3::fmax(max, p);
	}

	f32 half_area() {
		Vec3 e = max - min;
		return e.x * e.y + e.y * e.z + e.x * e.z;
	}

	Vec3 min;
	Vec3 max;
};

void AABB_grow(Vec3 &min, Vec3 &max, const Vec3 p) {
	min = Vec3::fmin(min, p);
	max = Vec3::fmax(max, p);
}

f32 AABB_half_area(const Vec3 &min, const Vec3 &max) {
	Vec3 e = max - min;
	return e.x * e.y + e.y * e.z + e.x * e.z;
}