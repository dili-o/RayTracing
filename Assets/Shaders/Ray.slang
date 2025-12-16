#pragma once

static const float PI = 3.1415926535897932385f;
static const float EPSILON = 1.192092896e-07f; 

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

