#pragma once
#include "HitRecord.slang"
#include "Interval.slang"

struct Sphere {
  float3 center;
  float  radius;

  uint material_index;
  uint material_type;
  uint padding[2];
};

void get_sphere_uv(float3 p, out float u, out float v) {
	// p: a given point on the sphere of radius one, centered at the origin.
	// u: returned value [0,1] of angle around the Y axis from X=-1.
	// v: returned value [0,1] of angle from Y=-1 to Y=+1.
	//     <1 0 0> yields <0.50 0.50>       <-1  0  0> yields <0.00 0.50>
	//     <0 1 0> yields <0.50 1.00>       < 0 -1  0> yields <0.50 0.00>
	//     <0 0 1> yields <0.25 0.50>       < 0  0 -1> yields <0.75 0.50>

	float theta = acos(-p.y);
	float phi = atan2(-p.z, p.x) + PI;

	u = phi / (2.f * PI);
	v = theta / PI;
}


bool sphere_hit(in Sphere sphere, in Ray r, Interval ray_t, inout HitRecord rec) {
    float3 oc = sphere.center - r.origin;
    float a = dot(r.direction, r.direction);
    float h = dot(r.direction, oc);
    float c = dot(oc, oc) - sphere.radius * sphere.radius;

    float discriminant = h * h - a * c;
    if (discriminant < 0.f)
      return false;

    float sqrtd = sqrt(discriminant);

    // Find the nearest root that lies in the acceptable range.
    float root = (h - sqrtd) / a;
    if (!ray_t.surrounds(root)) {
      root = (h + sqrtd) / a;
      if (!ray_t.surrounds(root))
        return false;
    }

    rec.t = root;
    rec.p = r.at(rec.t);
    float3 outward_normal = (rec.p - sphere.center) / sphere.radius;
    rec.set_face_normal(r, outward_normal);
    get_sphere_uv(outward_normal, rec.u, rec.v);
    rec.material_index = sphere.material_index;
    rec.material_type = sphere.material_type;

    return true;
}


