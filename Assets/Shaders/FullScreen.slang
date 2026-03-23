#pragma once

static float2 positions[3] = float2[](
  float2(0.0, -0.5),
  float2(0.5, 0.5),
  float2(-0.5, 0.5)
);

static float3 colors[3] = float3[](
  float3(1.0, 0.0, 0.0),
  float3(0.0, 1.0, 0.0),
  float3(0.0, 0.0, 1.0)
);

struct VertexOutput {
  float2 texCoord : TEXCOORD0;
  float4 sv_position : SV_Position;
  nointerpolation uint imageID;
};


[[vk::binding(0, 0)]]
Sampler2D image;

[shader("vertex")]
VertexOutput vertMain(uint vid : SV_VertexID, 
                      uint iid : SV_InstanceID) {
  VertexOutput output;
  output.texCoord = float2((vid << 1) & 2, vid & 2);
  output.sv_position = float4(output.texCoord.xy * 2.f - 1.f, 0.f, 1.f);
  output.imageID = iid;

  return output;
}


// AGX Tone Mapping Function
float3 agxToneMap(float3 color) {
  // Apply a basic curve for highlight compression
  const float a = 2.51f;
  const float b = 0.03f;
  const float c = 2.43f;
  const float d = 0.59f;
  const float e = 0.14f;

  return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.f, 1.f);
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert, uniform float total_rays) : SV_Target {
  float3 color = max(image.Sample(inVert.texCoord).xyz, 0.f);
  // float3 mapped = agxToneMap(average_color);
  // color = sqrt(color);
  return float4(agxToneMap(color), 1.f);
}
