#pragma once
#include "Interval.hpp"
#include "Ray.hpp"
#include "Vec3.hpp"

class AABB {
public:
	AABB() {}

	AABB(const Interval x, const Interval y, const Interval z)
		: x(x), y(y), z(z) {}

	AABB(const Point3& a, const Point3& b) {
		x = (a[0] <= b[0]) ? Interval(a[0], b[0]) : Interval(b[0], a[0]);
		y = (a[1] <= b[1]) ? Interval(a[1], b[1]) : Interval(b[1], a[1]);
		z = (a[2] <= b[2]) ? Interval(a[2], b[2]) : Interval(b[2], a[2]);
	}

	AABB(const AABB& box0, const AABB& box1) {
		x = Interval(box0.x, box1.x);
		y = Interval(box0.y, box1.y);
		z = Interval(box0.z, box1.z);
	}

	const Interval& axis_interval(i32 n) const {
		if (n == 1) return y;
		if (n == 2) return z;
		return x;
	}

	bool intersect(const Ray &r, Interval ray_t) const {
		const Point3& ray_orig = r.origin;
		const Vec3&   ray_dir  = r.direction;

		for (int axis = 0; axis < 3; axis++) {
			const Interval& ax = axis_interval(axis);
			const real adinv = 1.f / ray_dir[axis];

			real t0 = (ax.min - ray_orig[axis]) * adinv;
			real t1 = (ax.max - ray_orig[axis]) * adinv;

			if (t0 < t1) {
				if (t0 > ray_t.min) ray_t.min = t0;
				if (t1 < ray_t.max) ray_t.max = t1;
			} else {
				if (t1 > ray_t.min) ray_t.min = t1;
				if (t0 < ray_t.max) ray_t.max = t0;
			}

			if (ray_t.max <= ray_t.min)
				return false;
		}
		return true;
	}

public:
	Interval x, y, z;

	static AABB empty() {
		return AABB(Interval::empty(), Interval::empty(), Interval::empty());
	}

	static AABB universe() {
		return AABB(Interval::universe(), Interval::universe(), Interval::universe());
	}
};