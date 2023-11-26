struct PSInput
{
    float4 position : SV_POSITION;
    nointerpolation float4 color : COLOR;
};

[earlydepthstencil]
float4 PSMain(PSInput input) : SV_TARGET
{
    if (int(input.position.x) == 32 && int(input.position.y) == 31) {
        return float4(0.1f, 0.1f, 0.1f, 1.0f);
    }

    return input.color;
}

