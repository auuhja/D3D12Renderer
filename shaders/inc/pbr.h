#ifndef PBR_H
#define PBR_H

#include "light_probe.h"


static const float pi = 3.141592653589793238462643383279f;
static const float oneOverPI = 1.f / pi;

static const float3 ironAlbedo =		float3(0.560f, 0.570f, 0.580f);
static const float3 silverAlbedo =		float3(0.972f, 0.960f, 0.915f);
static const float3 aluminumAlbedo =	float3(0.913f, 0.921f, 0.925f);
static const float3 goldAlbedo =		float3(1.000f, 0.766f, 0.336f);
static const float3 copperAlbedo =		float3(0.955f, 0.637f, 0.538f);
static const float3 chromiumAlbedo =	float3(0.550f, 0.556f, 0.554f);
static const float3 nickelAlbedo =		float3(0.660f, 0.609f, 0.526f);
static const float3 titaniumAlbedo =	float3(0.542f, 0.497f, 0.449f);
static const float3 cobaltAlbedo =		float3(0.662f, 0.655f, 0.634f);
static const float3 platinumAlbedo =	float3(0.672f, 0.637f, 0.585f);

struct directional_light
{
	float4x4 vp[4];

	float4 worldSpaceDirection;
	float4 color;

	uint numShadowCascades;
	uint shadowMapDimensions;
	float shadowMapCascadeDistancePower;
	float cascadeBlendArea;
};

struct point_light
{
	float4 worldSpacePositionAndRadius;
	float4 color;
};


static float3 fresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (float3(1.f, 1.f, 1.f) - F0) * pow(1.f - cosTheta, 5.f);
}

static float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
	float v = 1.f - roughness;
	return F0 + (max(float3(v, v, v), F0) - F0) * pow(1.f - cosTheta, 5.f);
}

static float distributionGGX(float3 N, float3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.f);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.f) + 1.f);
	denom = pi * denom * denom;

	return nom / max(denom, 0.001f);
}

static float geometrySchlickGGX(float NdotV, float roughness)
{
	//float r = (roughness + 1.f);
	//float k = (r * r) * 0.125;
	float a = roughness;
	float k = (a * a) / 2.f;

	float nom = NdotV;
	float denom = NdotV * (1.f - k) + k;

	return nom / denom;
}

static float geometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.f);
	float NdotL = max(dot(N, L), 0.f);
	float ggx2 = geometrySchlickGGX(NdotV, roughness);
	float ggx1 = geometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

static float radicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

static float2 hammersley(uint i, uint N)
{
	return float2(float(i) / float(N), radicalInverse_VdC(i));
}

static float3 importanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.f * pi * Xi.x;
	float cosTheta = sqrt((1.f - Xi.y) / (1.f + (a * a - 1.f) * Xi.y));
	float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	// From spherical coordinates to cartesian coordinates.
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// From tangent-space vector to world-space sample vector.
	float3 up = abs(N.z) < 0.999 ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);

	float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

static float3 calculateAmbientLighting(float3 albedo, float3 irradiance,
	TextureCube<float4> environmentTexture, Texture2D<float4> brdf, SamplerState brdfSampler,
	float3 N, float3 V, float3 F0, float roughness, float metallic, float ao)
{
	// Common.
	float NdotV = max(dot(N, V), 0.f);
	float3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
	float3 kS = F;
	float3 kD = float3(1.f, 1.f, 1.f) - kS;
	kD *= 1.f - metallic;

	// Diffuse.
	float3 diffuse = irradiance * albedo;

	// Specular.
	float3 R = reflect(-V, N);
	uint width, height, numMipLevels;
	environmentTexture.GetDimensions(0, width, height, numMipLevels);
	float lod = roughness * float(numMipLevels - 1);

	float3 prefilteredColor = environmentTexture.SampleLevel(brdfSampler, R, lod).rgb;
	float2 envBRDF = brdf.Sample(brdfSampler, float2(roughness, NdotV)).rg;
	float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

	float3 ambient = (kD * diffuse + specular) * ao;

	return ambient;
}

static float3 calculateAmbientLighting(float3 albedo, 
	TextureCube<float4> irradianceTexture, TextureCube<float4> environmentTexture, Texture2D<float4> brdf, SamplerState brdfSampler,
	float3 N, float3 V, float3 F0, float roughness, float metallic, float ao)
{
	float3 irradiance = irradianceTexture.Sample(brdfSampler, N).rgb;
	return calculateAmbientLighting(albedo, irradiance, environmentTexture, brdf, brdfSampler, N, V, F0, roughness, metallic, ao);
}

static float3 calculateAmbientLighting(float3 albedo,
	StructuredBuffer<float4> lightProbePositions, StructuredBuffer<light_probe_tetrahedron> lightProbeTetrahedra, float3 worldPosition, uint tetrahedronIndex,
	StructuredBuffer<packed_spherical_harmonics> sphericalHarmonics,
	TextureCube<float4> environmentTexture, 
	Texture2D<float4> brdf, SamplerState brdfSampler,
	float3 N, float3 V, float3 F0, float roughness, float metallic, float ao)
{
	float4 barycentric;
	tetrahedronIndex = getEnclosingTetrahedron(lightProbePositions, lightProbeTetrahedra, worldPosition, tetrahedronIndex, barycentric);
	int4 shIndices = lightProbeTetrahedra[tetrahedronIndex].indices;
	float3 irradiance = sampleInterpolatedSphericalHarmonics(sphericalHarmonics, shIndices, barycentric, N).xyz;
	return calculateAmbientLighting(albedo, irradiance, environmentTexture, brdf, brdfSampler, N, V, F0, roughness, metallic, ao);
}

static float3 calculateAmbientLighting(float3 albedo,
	StructuredBuffer<float4> lightProbePositions, StructuredBuffer<light_probe_tetrahedron> lightProbeTetrahedra, float3 worldPosition, uint tetrahedronIndex,
	StructuredBuffer<spherical_harmonics> sphericalHarmonics,
	TextureCube<float4> environmentTexture,
	Texture2D<float4> brdf, SamplerState brdfSampler,
	float3 N, float3 V, float3 F0, float roughness, float metallic, float ao)
{
	float4 barycentric;
	tetrahedronIndex = getEnclosingTetrahedron(lightProbePositions, lightProbeTetrahedra, worldPosition, tetrahedronIndex, barycentric);
	int4 shIndices = lightProbeTetrahedra[tetrahedronIndex].indices;
	float3 irradiance = sampleInterpolatedSphericalHarmonics(sphericalHarmonics, shIndices, barycentric, N).xyz;
	return calculateAmbientLighting(albedo, irradiance, environmentTexture, brdf, brdfSampler, N, V, F0, roughness, metallic, ao);
}

static float3 calculateDirectLighting(float3 albedo, float3 radiance, float3 N, float3 L, float3 V, float3 F0, float roughness, float metallic)
{
	float3 H = normalize(V + L);
	float NdotV = max(dot(N, V), 0.f);

	// Cook-Torrance BRDF.
	float NDF = distributionGGX(N, H, roughness);
	float G = geometrySmith(N, V, L, roughness);
	float3 F = fresnelSchlick(max(dot(H, V), 0.f), F0);

	float3 kS = F;
	float3 kD = float3(1.f, 1.f, 1.f) - kS;
	kD *= 1.f - metallic;

	float NdotL = max(dot(N, L), 0.f);
	float3 numerator = NDF * G * F;
	float denominator = 4.f * NdotV * NdotL;
	float3 specular = numerator / max(denominator, 0.001f);

	return (kD * albedo * oneOverPI + specular) * radiance * NdotL;
}

#endif
