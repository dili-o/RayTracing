#include "Ray.slang"
#include "Sphere.slang"

float HitSphere(in float3 center, float radius, in Ray r) {
  float3 oc = center - r.origin;
  float a = dot(r.direction, r.direction);
  float h = dot(r.direction, oc);
  float c = dot(oc, oc) - radius * radius;
  float discriminant = h * h - a * c;

  if (discriminant < 0.f) {
    return -1.f;
  }

  return (h - sqrt(discriminant)) / a;
}

struct ShaderPushConstant {
  float4 pixel00Loc;
  float4 pixelDeltaU;
  float4 pixelDeltaV;
  float4 cameraCenter;
  uint imageWidth;
  uint imageHeight;
  uint sphereCount;
};

struct Uniforms{
  Sphere *spheres;
};

[[vk::binding(0, 0)]]
RWTexture2D<float4> outputImage;

[[vk::binding(1, 0)]]
StructuredBuffer<Uniforms> uniformBuffer;

[shader("compute")]
[numthreads(8,8,1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID,
                 uniform ShaderPushConstant pc) {
    int2 pixelCoord  = int2(dispatchThreadID.xy);

    if (pixelCoord.x < pc.imageWidth - 1 && pixelCoord.y < pc.imageHeight - 1) {
      float3 pixelCenter = pc.pixel00Loc.xyz + (pixelCoord.x * pc.pixelDeltaU.xyz)
         + (pixelCoord.y * pc.pixelDeltaV.xyz);
      float3 rayDirection = pixelCenter - pc.cameraCenter.xyz;
      Ray r = Ray(pc.cameraCenter.xyz, rayDirection);

      float3 rayColor;
      HitRecord rec;
      bool hit_sphere = false;
      float closest_so_far = 1000.f;
      for (uint i = 0; i < pc.sphereCount; ++i) {
        Sphere sphere = uniformBuffer[0].spheres[i];
        if(sphere_hit(sphere, r, 0.f, closest_so_far, rec)){
          hit_sphere = true;
          closest_so_far = rec.t;
        }
      }

      if (hit_sphere) {
        rayColor = 0.5f * (rec.normal + float3(1.f));
      } else {
        float3 unitDirection = normalize(r.direction);
        float a = 0.5f * (unitDirection.y + 1.f);
        rayColor = (1.f - a) * float3(1.f, 1.f, 1.f) + a * float3(0.5f, 0.7f, 1.f);
      }

      outputImage[pixelCoord] = float4(rayColor.r, rayColor.g, rayColor.b, 1.f);
    }
}
