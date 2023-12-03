#define ENABLE_CONSERVATIVE_RASTERIZATION_MODE  0

struct PSInput
{
    float4 position : SV_POSITION;
    linear centroid float4 color : COLOR;
};

RWBuffer<uint> uavOutput : register(u0, space0);

[earlydepthstencil]
// The `linear` interpolation will not apply!
float4 PSMain(PSInput input
#if ENABLE_CONSERVATIVE_RASTERIZATION_MODE
    , linear uint innerCoverage : SV_InnerCoverage
#endif
) : SV_TARGET
{
    InterlockedAdd(uavOutput[0], 1U);

    const float4 inputColor = input.color;

#if ENABLE_CONSERVATIVE_RASTERIZATION_MODE
    if (innerCoverage == 0U) {
        return float4(1.0f - inputColor.r, 1.0f - inputColor.g, 1.0f - inputColor.b, 1.0f);
    }
#endif

    if (int(input.position.x) == 32 && int(input.position.y) == 31) {
        return float4(0.1f, 0.1f, 0.1f, 1.0f);
    }

    return inputColor;
}

