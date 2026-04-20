
struct VertexInput {
  float4 position : POSITION;
};

struct VertexOutput {
  float4 position : SV_POSITION;
  nointerpolation uint sliceId : SLICEINDEX;
};

[[vk::binding(0, 0)]]
cbuffer CONSTANT_BUFFER {
  float4x4 gViewProjMat;

  float4 gColor;

  float3 gSunIlluminance;
  int gScatteringMaxPathDepth;

  uint2 gResolution;
  float gFrameTimeSec;
  float gTimeSec;

  uint2 gMouseLastDownPos;
  uint gFrameId;
  uint gTerrainResolution;

  float2 RayMarchMinMaxSPP;
  float gScreenshotCaptureActive;
  float pad;
};

[shader("vertex")]
VertexOutput ScreenTriangleVertexShader(uint vertexId: SV_VertexID,
                                        uint instanceId: SV_InstanceID) {
  VertexOutput output = (VertexOutput)0;

  // For a range on screen in [-0.5,0.5]
  float2 uv = -1.0f;
  uv = vertexId == 1 ? float2(-1.0f, 3.0f) : uv;
  uv = vertexId == 2 ? float2(3.0f, -1.0f) : uv;
  output.position = float4(uv, 0.0f, 1.0f);

  output.sliceId = instanceId;

  return output;
}
