struct VSInput {
  float2 position : POSITION;
  float2 tex_coord : TEXCOORD0;
  float4 color_unorm : COLOR; // NOTE: This should be changed to a uint
};

struct VSOutput {
  float4 position : SV_POSITION;
  float4 color : COLOR;
  float2 tex_coord : TEXCOORD0;
};

[shader("vertex")]
VSOutput vert_main(VSInput input,
									uniform float4 scale_translate) {
  VSOutput output;
  output.tex_coord = input.tex_coord;
#ifdef LINEAR_SPACE
  output.color = input.color_unorm;
#else
  // Approximation sRGB to Linear conversion
  output.color = float4(pow(input.color_unorm.rgb, float3(2.2f)), input.color_unorm.a); 
#endif
  float2 scale = scale_translate.xy;
  float2 translate = scale_translate.zw;
  output.position = float4(input.position * scale + translate, 0.f, 1.f);

  return output;
}

[[vk::binding(0, 0)]]
Sampler2D input_texture;

[shader("fragment")]
float4 frag_main(VSOutput input) {
  return input_texture.Sample(input.tex_coord.xy) * input.color;
}

