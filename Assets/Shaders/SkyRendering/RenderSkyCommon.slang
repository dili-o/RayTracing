#include "SkyAtmosphereCommon.slang"

#define DEBUGENABLED 0

#define PLANET_RADIUS_OFFSET 0.01f

#ifndef MULTISCATAPPROX_ENABLED 
#define MULTISCATAPPROX_ENABLED 0 
#endif
#ifndef SHADOWMAP_ENABLED 
#define SHADOWMAP_ENABLED 0 
#endif


[[vk::binding(0, 1)]]
Sampler2D<float4>  TransmittanceLutTexture;

////////////////////////////////////////////////////////////
// Participating media
////////////////////////////////////////////////////////////

float getAlbedo(float scattering, float extinction) {
  return scattering / max(0.001, extinction);
}
float3 getAlbedo(float3 scattering, float3 extinction) {
  return scattering / max(0.001, extinction);
}

struct MediumSampleRGB {
  float3 scattering;
  float3 absorption;
  float3 extinction;

  float3 scatteringMie;
  float3 absorptionMie;
  float3 extinctionMie;

  float3 scatteringRay;
  float3 absorptionRay;
  float3 extinctionRay;

  float3 scatteringOzo;
  float3 absorptionOzo;
  float3 extinctionOzo;

  float3 albedo;
};

MediumSampleRGB sampleMediumRGB(in float3 WorldPos,
                                in AtmosphereParameters Atmosphere) {
  const float viewHeight = length(WorldPos) - Atmosphere.BottomRadius;

  const float densityMie = exp(Atmosphere.MieDensityExpScale * viewHeight);
  const float densityRay = exp(Atmosphere.RayleighDensityExpScale * viewHeight);
  const float densityOzo =
      saturate(viewHeight < Atmosphere.AbsorptionDensity0LayerWidth
                   ? Atmosphere.AbsorptionDensity0LinearTerm * viewHeight +
                         Atmosphere.AbsorptionDensity0ConstantTerm
                   : Atmosphere.AbsorptionDensity1LinearTerm * viewHeight +
                         Atmosphere.AbsorptionDensity1ConstantTerm);

  MediumSampleRGB s;

  s.scatteringMie = densityMie * Atmosphere.MieScattering;
  s.absorptionMie = densityMie * Atmosphere.MieAbsorption;
  s.extinctionMie = densityMie * Atmosphere.MieExtinction;

  s.scatteringRay = densityRay * Atmosphere.RayleighScattering;
  s.absorptionRay = 0.0f;
  s.extinctionRay = s.scatteringRay + s.absorptionRay;

  s.scatteringOzo = 0.0;
  s.absorptionOzo = densityOzo * Atmosphere.AbsorptionExtinction;
  s.extinctionOzo = s.scatteringOzo + s.absorptionOzo;

  s.scattering = s.scatteringMie + s.scatteringRay + s.scatteringOzo;
  s.absorption = s.absorptionMie + s.absorptionRay + s.absorptionOzo;
  s.extinction = s.extinctionMie + s.extinctionRay + s.extinctionOzo;
  s.albedo = getAlbedo(s.scattering, s.extinction);

  return s;
}

////////////////////////////////////////////////////////////
// LUT functions
////////////////////////////////////////////////////////////

// Transmittance LUT function parameterisation from Bruneton 2017
// https://github.com/ebruneton/precomputed_atmospheric_scattering uv in [0,1]
// viewZenithCosAngle in [-1,1]
// viewHeight in [bottomRAdius, topRadius]

// We should precompute those terms from resolutions (Or set resolution as
// #defined constants)
float fromUnitToSubUvs(float u, float resolution) {
  return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f));
}
float fromSubUvsToUnit(float u, float resolution) {
  return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f));
}

void UvToLutTransmittanceParams(AtmosphereParameters Atmosphere,
                                out float viewHeight,
                                out float viewZenithCosAngle, in float2 uv) {
  // uv = float2(fromSubUvsToUnit(uv.x, TRANSMITTANCE_TEXTURE_WIDTH),
  // fromSubUvsToUnit(uv.y, TRANSMITTANCE_TEXTURE_HEIGHT)); // No real impact so
  // off
  float x_mu = uv.x;
  float x_r = uv.y;

  float H = sqrt(Atmosphere.TopRadius * Atmosphere.TopRadius -
                 Atmosphere.BottomRadius * Atmosphere.BottomRadius);
  float rho = H * x_r;
  viewHeight =
      sqrt(rho * rho + Atmosphere.BottomRadius * Atmosphere.BottomRadius);

  float d_min = Atmosphere.TopRadius - viewHeight;
  float d_max = rho + H;
  float d = d_min + x_mu * (d_max - d_min);
  viewZenithCosAngle =
      d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * viewHeight * d);
  viewZenithCosAngle = clamp(viewZenithCosAngle, -1.0, 1.0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

float RayleighPhase(float cosTheta) {
  float factor = 3.0f / (16.0f * PI);
  return factor * (1.0f + cosTheta * cosTheta);
}

float CornetteShanksMiePhaseFunction(float g, float cosTheta) {
  float k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
  return k * (1.0 + cosTheta * cosTheta) /
         pow(1.0 + g * g - 2.0 * g * -cosTheta, 1.5);
}

float hgPhase(float g, float cosTheta) {
#ifdef USE_CornetteShanks
  return CornetteShanksMiePhaseFunction(g, cosTheta);
#else
  // Reference implementation (i.e. not schlick approximation).
  // See http://www.pbr-book.org/3ed-2018/Volume_Scattering/Phase_Functions.html
  float numer = 1.0f - g * g;
  float denom = 1.0f + g * g + 2.0f * g * cosTheta;
  return numer / (4.0f * PI * denom * sqrt(denom));
#endif
}

float mean(float3 v)
{
	return dot(v, float3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f));
}

float whangHashNoise(uint u, uint v, uint s)
{
	uint seed = (u * 1664525u + v) + s;
	seed = (seed ^ 61u) ^ (seed >> 16u);
	seed *= 9u;
	seed = seed ^ (seed >> 4u);
	seed *= uint(0x27d4eb2d);
	seed = seed ^ (seed >> 15u);
	float value = float(seed) / (4294967296.0);
	return value;
}
