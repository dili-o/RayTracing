#pragma once
#include "HitRecord.slang"
#include "Interval.slang"

struct Sphere {
  float3 center;
  float  radius;
};

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

    return true;
}


