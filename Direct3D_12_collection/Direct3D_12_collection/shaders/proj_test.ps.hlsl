
struct PSInput
{
    float4 position : SV_POSITION;
    linear float4 color : COLOR;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    if (int(input.position.x) == 32 && int(input.position.y) == 31) {
        return float4(0.1f, 0.1f, 0.1f, 1.0f);
    }

    return input.color;
}

