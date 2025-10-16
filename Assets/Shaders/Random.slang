#pragma once

// Random Number Generators
uint wang_hash(inout uint seed)
{
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

// Range 0 to 1
float rand(inout uint seed) {
    return float(wang_hash(seed)) / 4294967296.0;
}

float rand_range(float min, float max, inout uint seed) {
  return min + (max - min) * rand(seed);
}
