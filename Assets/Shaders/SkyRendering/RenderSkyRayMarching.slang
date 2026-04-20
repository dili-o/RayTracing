#include "RenderSkyCommon.slang"

struct SingleScatteringResult {
  float3 L;             // Scattered light (luminance)
  float3 OpticalDepth;  // Optical depth (1/m)
  float3 Transmittance; // Transmittance in [0,1] (unitless)
  float3 MultiScatAs1;

  float3 NewMultiScatStep0Out;
  float3 NewMultiScatStep1Out;
};

SingleScatteringResult IntegrateScatteredLuminance(
    in float2 pixPos, in float3 WorldPos, in float3 WorldDir, in float3 SunDir,
    in AtmosphereParameters Atmosphere, in bool ground, in float SampleCountIni,
    in float DepthBufferValue, in bool VariableSampleCount, in bool MieRayPhase,
    in float tMaxMax = 9000000.0f) {
  const bool debugEnabled = all(uint2(pixPos.xx) == gMouseLastDownPos.xx) &&
                            uint(pixPos.y) % 10 == 0 &&
                            DepthBufferValue != -1.0f;
  SingleScatteringResult result = (SingleScatteringResult)0;

  float3 ClipSpace = float3((pixPos / float2(gResolution)) * float2(2.0, -2.0) -
                                float2(1.0, -1.0),
                            1.0);

  // Compute next intersection with atmosphere or ground
  float3 earthO = float3(0.0f, 0.0f, 0.0f);
  float tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO,
                                            Atmosphere.BottomRadius);
  float tTop = raySphereIntersectNearest(WorldPos, WorldDir, earthO,
                                         Atmosphere.TopRadius);
  float tMax = 0.0f;
  if (tBottom < 0.0f) {
    if (tTop < 0.0f) {
      tMax = 0.0f; // No intersection with earth nor atmosphere: stop right away
      return result;
    } else {
      tMax = tTop;
    }
  } else {
    if (tTop > 0.0f) {
      tMax = min(tTop, tBottom);
    }
  }

  if (DepthBufferValue >= 0.0f) {
    ClipSpace.z = DepthBufferValue;
    if (ClipSpace.z < 1.0f) {
      float4 DepthBufferWorldPos =
          mul(gSkyInvViewProjMat, float4(ClipSpace, 1.0));
      DepthBufferWorldPos /= DepthBufferWorldPos.w;

      float tDepth = length(
          DepthBufferWorldPos.xyz -
          (WorldPos +
           float3(
               0.0, 0.0,
               -Atmosphere.BottomRadius))); // apply earth offset to go back to
                                            // origin as top of earth mode.
      if (tDepth < tMax) {
        tMax = tDepth;
      }
    }
    //		if (VariableSampleCount && ClipSpace.z == 1.0f)
    //			return result;
  }
  tMax = min(tMax, tMaxMax);

  // Sample count
  float SampleCount = SampleCountIni;
  float SampleCountFloor = SampleCountIni;
  float tMaxFloor = tMax;
  if (VariableSampleCount) {
    SampleCount =
        lerp(RayMarchMinMaxSPP.x, RayMarchMinMaxSPP.y, saturate(tMax * 0.01));
    SampleCountFloor = floor(SampleCount);
    tMaxFloor =
        tMax * SampleCountFloor /
        SampleCount; // rescale tMax to map to the last entire step segment.
  }
  float dt = tMax / SampleCount;

  // Phase functions
  const float uniformPhase = 1.0 / (4.0 * PI);
  const float3 wi = SunDir;
  const float3 wo = WorldDir;
  float cosTheta = dot(wi, wo);
  float MiePhaseValue = hgPhase(Atmosphere.MiePhaseG,
                                -cosTheta); // mnegate cosTheta because due to
                                            // WorldDir being a "in" direction.
  float RayleighPhaseValue = RayleighPhase(cosTheta);

#ifdef ILLUMINANCE_IS_ONE
  // When building the scattering factor, we assume light illuminance is 1 to
  // compute a transfert function relative to identity illuminance of 1. This
  // make the scattering factor independent of the light. It is now only linked
  // to the atmosphere properties.
  float3 globalL = 1.0f;
#else
  float3 globalL = gSunIlluminance;
#endif

  // Ray march the atmosphere to integrate optical depth
  float3 L = 0.0f;
  float3 throughput = 1.0;
  float3 OpticalDepth = 0.0;
  float t = 0.0f;
  float tPrev = 0.0;
  const float SampleSegmentT = 0.3f;
  for (float s = 0.0f; s < SampleCount; s += 1.0f) {
    if (VariableSampleCount) {
      // More expenssive but artefact free
      float t0 = (s) / SampleCountFloor;
      float t1 = (s + 1.0f) / SampleCountFloor;
      // Non linear distribution of sample within the range.
      t0 = t0 * t0;
      t1 = t1 * t1;
      // Make t0 and t1 world space distances.
      t0 = tMaxFloor * t0;
      if (t1 > 1.0) {
        t1 = tMax;
        //	t1 = tMaxFloor;	// this reveal depth slices
      } else {
        t1 = tMaxFloor * t1;
      }
      // t = t0 + (t1 - t0) * (whangHashNoise(pixPos.x, pixPos.y, gFrameId *
      // 1920 * 1080)); // With dithering required to hide some sampling
      // artefact relying on TAA later? This may even allow volumetric shadow?
      t = t0 + (t1 - t0) * SampleSegmentT;
      dt = t1 - t0;
    } else {
      // t = tMax * (s + SampleSegmentT) / SampleCount;
      //  Exact difference, important for accuracy of multiple scattering
      float NewT = tMax * (s + SampleSegmentT) / SampleCount;
      dt = NewT - t;
      t = NewT;
    }
    float3 P = WorldPos + t * WorldDir;

#if DEBUGENABLED
    if (debugEnabled) {
      float3 Pprev = WorldPos + tPrev * WorldDir;
      float3 TxToDebugWorld = float3(0, 0, -Atmosphere.BottomRadius);
      addGpuDebugLine(TxToDebugWorld + Pprev, TxToDebugWorld + P,
                      float3(0.2, 1, 0.2));
      addGpuDebugCross(TxToDebugWorld + P, float3(0.2, 0.2, 1.0), 0.2);
    }
#endif

    MediumSampleRGB medium = sampleMediumRGB(P, Atmosphere);
    const float3 SampleOpticalDepth = medium.extinction * dt;
    const float3 SampleTransmittance = exp(-SampleOpticalDepth);
    OpticalDepth += SampleOpticalDepth;

    float pHeight = length(P);
    const float3 UpVector = P / pHeight;
    float SunZenithCosAngle = dot(SunDir, UpVector);
    float2 uv;
    LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);
    float3 TransmittanceToSun =
        TransmittanceLutTexture.SampleLevel(uv, 0).rgb;

    float3 PhaseTimesScattering;
    if (MieRayPhase) {
      PhaseTimesScattering = medium.scatteringMie * MiePhaseValue +
                             medium.scatteringRay * RayleighPhaseValue;
    } else {
      PhaseTimesScattering = medium.scattering * uniformPhase;
    }

    // Earth shadow
    float tEarth = raySphereIntersectNearest(
        P, SunDir, earthO + PLANET_RADIUS_OFFSET * UpVector,
        Atmosphere.BottomRadius);
    float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

    // Dual scattering for multi scattering

    float3 multiScatteredLuminance = 0.0f;
#if MULTISCATAPPROX_ENABLED
    multiScatteredLuminance = GetMultipleScattering(
        Atmosphere, medium.scattering, medium.extinction, P, SunZenithCosAngle);
#endif

    float shadow = 1.0f;
#if SHADOWMAP_ENABLED
    // First evaluate opaque shadow
    shadow = getShadow(Atmosphere, P);
#endif

    float3 S = globalL * (earthShadow * shadow * TransmittanceToSun *
                              PhaseTimesScattering +
                          multiScatteredLuminance * medium.scattering);

    // When using the power serie to accumulate all sattering order, serie r
    // must be <1 for a serie to converge. Under extreme coefficient,
    // MultiScatAs1 can grow larger and thus result in broken visuals. The way
    // to fix that is to use a proper analytical integration as proposed in
    // slide 28 of
    // http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
    // However, it is possible to disable as it can also work using simple power
    // serie sum unroll up to 5th order. The rest of the orders has a really low
    // contribution.
#define MULTI_SCATTERING_POWER_SERIE 1

#if MULTI_SCATTERING_POWER_SERIE == 0
    // 1 is the integration of luminance over the 4pi of a sphere, and assuming
    // an isotropic phase function of 1.0/(4*PI)
    result.MultiScatAs1 += throughput * medium.scattering * 1 * dt;
#else
    float3 MS = medium.scattering * 1;
    float3 MSint = (MS - MS * SampleTransmittance) / medium.extinction;
    result.MultiScatAs1 += throughput * MSint;
#endif

    // Evaluate input to multi scattering
    {
      float3 newMS;

      newMS = earthShadow * TransmittanceToSun * medium.scattering *
              uniformPhase * 1;
      result.NewMultiScatStep0Out += throughput *
                                     (newMS - newMS * SampleTransmittance) /
                                     medium.extinction;
      //	result.NewMultiScatStep0Out += SampleTransmittance * throughput
      //* newMS * dt;

      newMS = medium.scattering * uniformPhase * multiScatteredLuminance;
      result.NewMultiScatStep1Out += throughput *
                                     (newMS - newMS * SampleTransmittance) /
                                     medium.extinction;
      //	result.NewMultiScatStep1Out += SampleTransmittance * throughput
      //* newMS * dt;
    }

#if 0
		L += throughput * S * dt;
		throughput *= SampleTransmittance;
#else
    // See slide 28 at
    // http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
    float3 Sint = (S - S * SampleTransmittance) /
                  medium.extinction; // integrate along the current step segment
    L += throughput * Sint; // accumulate and also take into account the
                            // transmittance from previous steps
    throughput *= SampleTransmittance;
#endif

    tPrev = t;
  }

  if (ground && tMax == tBottom && tBottom > 0.0) {
    // Account for bounced light off the earth
    float3 P = WorldPos + tBottom * WorldDir;
    float pHeight = length(P);

    const float3 UpVector = P / pHeight;
    float SunZenithCosAngle = dot(SunDir, UpVector);
    float2 uv;
    LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);
    float3 TransmittanceToSun =
        TransmittanceLutTexture.SampleLevel(uv, 0).rgb;

    const float NdotL = saturate(dot(normalize(UpVector), normalize(SunDir)));
    L += globalL * TransmittanceToSun * throughput * NdotL *
         Atmosphere.GroundAlbedo / PI;
  }

  result.L = L;
  result.OpticalDepth = OpticalDepth;
  result.Transmittance = throughput;
  return result;
}

[shader("fragment")]
float4 RenderTransmittanceLutPS(VertexOutput Input) : SV_TARGET {
  float2 pixPos = Input.position.xy;
  AtmosphereParameters Atmosphere = GetAtmosphereParameters();

  // Compute camera position from LUT coords
  float2 uv = (pixPos) / float2(TRANSMITTANCE_TEXTURE_WIDTH,
                                TRANSMITTANCE_TEXTURE_HEIGHT);
  float viewHeight;
  float viewZenithCosAngle;
  UvToLutTransmittanceParams(Atmosphere, viewHeight, viewZenithCosAngle, uv);

  //  A few extra needed constants
  float3 WorldPos = float3(0.0f, 0.0f, viewHeight);
  float3 WorldDir =
      float3(0.0f, sqrt(1.0 - viewZenithCosAngle * viewZenithCosAngle),
             viewZenithCosAngle);

  const bool ground = false;
  const float SampleCountIni =
      40.0f; // Can go a low as 10 sample but energy lost starts to be visible.
  const float DepthBufferValue = -1.0;
  const bool VariableSampleCount = false;
  const bool MieRayPhase = false;
  float3 transmittance =
      exp(-IntegrateScatteredLuminance(pixPos, WorldPos, WorldDir,
                                       sun_direction, Atmosphere, ground,
                                       SampleCountIni, DepthBufferValue,
                                       VariableSampleCount, MieRayPhase)
               .OpticalDepth);

  // Opetical depth to transmittance
  return float4(transmittance, 1.0f);
}
