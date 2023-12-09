#define ENABLE_CONSERVATIVE_RASTERIZATION_MODE  0
#define ENABLE_SAMPLE_INTERPOLATION             0

struct PSInput
{
    linear centroid float4 position : SV_POSITION;
    linear centroid float4 color : COLOR;
    //sample float4 color : COLOR;
};

RWBuffer<uint> uavOutput : register(u0, space0);

[earlydepthstencil]
float4 PSMain(PSInput input, out float outDepth : SV_Depth,
    uint shadingRate : SV_ShadingRate
#if ENABLE_CONSERVATIVE_RASTERIZATION_MODE
    // The `linear` interpolation will not apply!
    , linear uint innerCoverage : SV_InnerCoverage
#endif
#if ENABLE_SAMPLE_INTERPOLATION
    , uint sampleIndex : SV_SampleIndex
#endif
            ) : SV_TARGET
{
    InterlockedAdd(uavOutput[0], 1U);

    outDepth = 1.5f;

    const float4 inputColor = input.color;

#if ENABLE_CONSERVATIVE_RASTERIZATION_MODE
    if (innerCoverage == 0U) {
        return float4(1.0f - inputColor.r, 1.0f - inputColor.g, 1.0f - inputColor.b, 1.0f);
    }
#endif

    if (int(input.position.x) == 32 && int(input.position.y) == 31)
    {
        enum D3D12_SHADING_RATE
        {
            D3D12_SHADING_RATE_1X1 = 0,
            D3D12_SHADING_RATE_1X2 = 0x1,
            D3D12_SHADING_RATE_2X1 = 0x4,
            D3D12_SHADING_RATE_2X2 = 0x5,
            D3D12_SHADING_RATE_2X4 = 0x6,
            D3D12_SHADING_RATE_4X2 = 0x9,
            D3D12_SHADING_RATE_4X4 = 0xa
        };

        switch (shadingRate)
        {
        case D3D12_SHADING_RATE_1X1:
        default:
            return float4(0.1f, 0.1f, 0.1, 1.0f);

        case D3D12_SHADING_RATE_1X2:
            return float4(0.9f, 0.1f, 0.1, 1.0f);

        case D3D12_SHADING_RATE_2X1:
            return float4(0.1f, 0.9f, 0.1, 1.0f);

        case D3D12_SHADING_RATE_2X2:
            return float4(0.9f, 0.9f, 0.9, 1.0f);
        }
    }

#if ENABLE_SAMPLE_INTERPOLATION
    switch (sampleIndex)
    {
    case 0:
    default:
        return float4(0.9f, 0.1f, 0.1f, 1.0f);
    case 1:
        return float4(0.1f, 0.9f, 0.1f, 1.0f);
    case 2:
        return float4(0.1f, 0.1f, 0.9f, 1.0f);
    case 3:
        return float4(0.9f, 0.9f, 0.1f, 1.0f);
    }
#endif

    if (frac(input.position.x) != 0.5f || frac(input.position.y) != 0.5f) {
        return float4(0.9f, 0.9f, 0.9f, 1.0f);
    }

    return inputColor;
}

