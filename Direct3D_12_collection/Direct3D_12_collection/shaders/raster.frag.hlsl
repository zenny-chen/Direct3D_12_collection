
#define ENABLE_SAMPLE_INTERPOLATION             0

struct PSInput
{
    linear centroid float4 position : SV_POSITION;
    nointerpolation float4 color : COLOR;
    //sample float4 color : COLOR;
};

//RWBuffer<uint> uavOutput : register(u0, space0);
// Intel HD Graphics and Iris Pro Graphics DO NOT support RWBuffer.
RWStructuredBuffer<uint> uavOutput : register(u0, space0);

[earlydepthstencil]
float4 PSMain(PSInput input //, out float outDepth : SV_Depth,
#if ENABLE_SAMPLE_INTERPOLATION
    , uint sampleIndex : SV_SampleIndex
#endif
) : SV_TARGET
{
    float4 inputColor = input.color;

    InterlockedAdd(uavOutput[0], 1U);

#if ENABLE_SAMPLE_INTERPOLATION
    switch (sampleIndex)
    {
    case 0:
    default:
        inputColor = float4(0.9f, 0.1f, 0.1f, 1.0f);
        break;
    case 1:
        inputColor = float4(0.1f, 0.9f, 0.1f, 1.0f);
        break;
    case 2:
        inputColor = float4(0.1f, 0.1f, 0.9f, 1.0f);
        break;
    case 3:
        inputColor = float4(0.9f, 0.9f, 0.1f, 1.0f);
        break;
    }
#endif

    return inputColor;
}

