struct PSInput
{
    float4 position : SV_POSITION;
    nointerpolation float4 color : COLOR;
};

[earlydepthstencil]
// The `linear` interpolation will not apply!
float4 PSMain(PSInput input, linear uint innerCoverage : SV_InnerCoverage) : SV_TARGET
{
    if (innerCoverage == 0U) return float4(0.9f, 0.9f, 0.9f, 1.0f);    // input.color + float4(0.05f, 0.05f, 0.05f, 0.0f);

    if (int(input.position.x) == 32 && int(input.position.y) == 31) {
        return float4(0.1f, 0.1f, 0.1f, 1.0f);
    }

    return input.color;
}

