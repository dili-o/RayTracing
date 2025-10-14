
struct Ray {
  __init(in float3 origin_, in float3 direction_) {
    origin = origin_;
    direction = direction_;
  }

  float3 origin;
  float3 direction;
};

bool HitSphere(in float3 center, float radius, in Ray r) {
  float3 oc = center - r.origin;
  float a = dot(r.direction, r.direction);
  float b = -2.f * dot(r.direction, oc);
  float c = dot(oc, oc) - radius * radius;
  float discriminant = b * b - 4 * a * c;
  return (discriminant >= 0);
}

float3 RayColor(in Ray ray) {
  if (HitSphere(float3(0.f, 0.f, -1.f), 0.5f, ray))
    return float3(1.f, 0.f, 0.f);
  float3 unitDirection = normalize(ray.direction);
  float a = 0.5f * (unitDirection.y + 1.f);
  return (1.f - a) * float3(1.f, 1.f, 1.f) + a * float3(0.5f, 0.7f, 1.f);
}


[[vk::binding(0, 0)]]
RWTexture2D<float4> outputImage;

[shader("compute")]
[numthreads(8,8,1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID,
                 uniform float4 pixel00Loc, uniform float4 pixelDeltaU,
                 uniform float4 pixelDeltaV, uniform float4 cameraCenter,
                 uniform uint imageWidth, uniform uint imageHeight) {
    int2 pixelCoord  = int2(dispatchThreadID.xy);
    if (pixelCoord.x < imageWidth && pixelCoord.y < imageHeight) {
      float3 pixelCenter = pixel00Loc.xyz + (pixelCoord.x * pixelDeltaU.xyz)
         + (pixelCoord.y * pixelDeltaV.xyz);
      float3 rayDirection = pixelCenter - cameraCenter.xyz;
      Ray r = Ray(cameraCenter.xyz, rayDirection);

      float3 rayColor = RayColor(r);

      outputImage[pixelCoord] = float4(rayColor.r, rayColor.g, rayColor.b, 1.f);
    }
}
