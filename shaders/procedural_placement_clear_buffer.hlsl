
struct cs_input
{
	uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
	uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
	uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
	uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
};

cbuffer clear_cb : register(b0)
{
	uint size;
}

RWStructuredBuffer<uint> buffer	: register(u0);

[numthreads(512, 1, 1)]
void main(cs_input IN)
{
	if (IN.dispatchThreadID.x < size)
	{
		buffer[IN.dispatchThreadID.x] = 0;
	}
}

