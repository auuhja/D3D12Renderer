#define BLOCK_SIZE 16

struct cs_input
{
	uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
	uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
	uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
	uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
};

cbuffer prefilter_environment_cb : register(b0)
{
	uint cubemapSize;				// Size of the cubemap face in pixels at the current mipmap level.
	uint firstMip;					// The first mip level to generate.
	uint numMipLevelsToGenerate;	// The number of mips to generate.
	uint totalNumMipLevels;
};

TextureCube<float4> srcTexture : register(t0);

RWTexture2DArray<float4> outMip1 : register(u0);
RWTexture2DArray<float4> outMip2 : register(u1);
RWTexture2DArray<float4> outMip3 : register(u2);
RWTexture2DArray<float4> outMip4 : register(u3);
RWTexture2DArray<float4> outMip5 : register(u4);

SamplerState linearRepeatSampler : register(s0);

#define prefilterEnvironment_rootSignature \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 4), " \
    "DescriptorTable( SRV(t0, numDescriptors = 1) )," \
    "DescriptorTable( UAV(u0, numDescriptors = 5) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_MIN_MAG_LINEAR_MIP_POINT )"

static const float pi = 3.141592653589793238462643383279f;

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

	// from spherical coordinates to cartesian coordinates
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space vector to world-space sample vector
	float3 up = abs(N.z) < 0.999 ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);

	float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}


// Transform from dispatch ID to cubemap face direction
static const float3x3 rotateUV[6] = {
	// +X
	float3x3(0,  0,  1,
			 0, -1,  0,
			 -1,  0,  0),
	// -X
    float3x3(0,  0, -1,
    		 0, -1,  0,
    		 1,  0,  0),
	// +Y
	float3x3(1,  0,  0,
	         0,  0,  1,
			 0,  1,  0),
	// -Y
	float3x3(1,  0,  0,
	    	 0,  0, -1,
			 0, -1,  0),
	// +Z
	float3x3(1,  0,  0,
			 0, -1,  0,
			 0,  0,  1),
	// -Z
	float3x3(-1,  0,  0,
		     0,  -1,  0,
			 0,   0, -1)
};


static float4 filter(uint mip, float3 N, float3 V)
{
	float relMipLevel = float(mip) / (totalNumMipLevels - 1);

	const uint SAMPLE_COUNT = 1024u;
	float totalWeight = 0.f;
	float3 prefilteredColor = float3(0.f, 0.f, 0.f);
	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		float2 Xi = hammersley(i, SAMPLE_COUNT);
		float3 H = importanceSampleGGX(Xi, N, relMipLevel);
		float3 L = normalize(2.f * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0.f);
		if (NdotL > 0.f)
		{
			prefilteredColor += srcTexture.SampleLevel(linearRepeatSampler, L, 0).xyz * NdotL;
			totalWeight += NdotL;
		}
	}
	prefilteredColor = prefilteredColor / totalWeight;
	return float4(prefilteredColor, 1.f);
}

[RootSignature(prefilterEnvironment_rootSignature)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	// Cubemap texture coords.
	uint3 texCoord = IN.dispatchThreadID;

	// First check if the thread is in the cubemap dimensions.
	if (texCoord.x >= cubemapSize || texCoord.y >= cubemapSize) return;

	// Map the UV coords of the cubemap face to a direction
	// [(0, 0), (1, 1)] => [(-0.5, -0.5), (0.5, 0.5)]
	float3 N = float3(texCoord.xy / float(cubemapSize) - 0.5f, 0.5f);
	N = normalize(mul(rotateUV[texCoord.z], N));

	float3 R = N;
	float3 V = R;

	outMip1[texCoord] = filter(firstMip, N, V);

	if (numMipLevelsToGenerate > 1 && (IN.groupIndex & 0x11) == 0)
	{
		outMip2[uint3(texCoord.xy / 2, texCoord.z)] = filter(firstMip + 1, N, V);
	}

	if (numMipLevelsToGenerate > 2 && (IN.groupIndex & 0x33) == 0)
	{
		outMip3[uint3(texCoord.xy / 4, texCoord.z)] = filter(firstMip + 2, N, V);
	}

	if (numMipLevelsToGenerate > 3 && (IN.groupIndex & 0x77) == 0)
	{
		outMip4[uint3(texCoord.xy / 8, texCoord.z)] = filter(firstMip + 3, N, V);
	}

	if (numMipLevelsToGenerate > 4 && (IN.groupIndex & 0xFF) == 0)
	{
		outMip5[uint3(texCoord.xy / 16, texCoord.z)] = filter(firstMip + 4, N, V);
	}
}