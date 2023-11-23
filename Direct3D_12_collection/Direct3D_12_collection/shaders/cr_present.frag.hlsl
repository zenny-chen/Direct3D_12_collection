struct PSInput
{
    float4 position : SV_POSITION;
};

Texture2D<float4> rtTexture : register(t0, space0);

float4 PSMain(PSInput input) : SV_TARGET
{
    const int x = int(input.position.x) / 8;
    const int y = int(input.position.y) / 8;

    return rtTexture.Load(int3(x, y, 0));
}

