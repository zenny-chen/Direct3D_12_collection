#define ENABLE_WRITE_DEPTH  0

struct PSInput
{
    centroid float4 position : SV_POSITION;
    centroid float4 color : COLOR;
};

RWBuffer<uint> uavOutput : register(u0, space0);

//[earlydepthstencil]
float4 PSMain(PSInput input
#if ENABLE_WRITE_DEPTH
    , out float outDepth : SV_Depth
#endif
) : SV_TARGET
{
#if ENABLE_WRITE_DEPTH
    outDepth = 0.9f;
#endif

    InterlockedAdd(uavOutput[0], 1U);

    return input.color;
}

