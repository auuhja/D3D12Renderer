struct ps_input
{
	float3 uv	: TEXCOORDS;
};

SamplerState texSampler	: register(s0);
TextureCube<float4> tex	: register(t0);

float4 main(ps_input IN) : SV_TARGET
{
	return tex.Sample(texSampler, IN.uv);
}
