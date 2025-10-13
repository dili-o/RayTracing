[[vk::binding(0, 0)]]
RWTexture2D<float4> outputImage;

[shader("compute")]
[numthreads(8,8,1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID,
                 uniform uint imageWidth, uniform uint imageHeight) {
    int2 pixelCoord  = int2(dispatchThreadID.xy);

    float r = float(pixelCoord.x) / (imageWidth  - 1);
    float g = float(pixelCoord.y) / (imageHeight - 1);

    outputImage[pixelCoord] = float4(r, g, 0.f, 1.f);
}
