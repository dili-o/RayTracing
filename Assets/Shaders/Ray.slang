#pragma once

struct Ray {
  __init(in float3 origin_, in float3 direction_) {
    origin = origin_;
    direction = direction_;
  }

  float3 at(float t) {
    return origin + t * direction;
  }

  float3 origin;
  float3 direction;
};

