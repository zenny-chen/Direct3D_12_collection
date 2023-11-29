#define USE_MSAA                0
#define USE_DEPTH_STENCIL       0

#if USE_DEPTH_STENCIL
#define TEXEL_TYPE              float
#else
#define TEXEL_TYPE              float4
#endif

struct PSInput
{
    float4 position : SV_POSITION;
};

#if USE_MSAA
Texture2DMS<TEXEL_TYPE> rtTextureMS : register(t0, space0);
#else
Texture2D<TEXEL_TYPE> rtTexture : register(t0, space0);
#endif

float4 PSMain(PSInput input) : SV_TARGET
{
    const int x = int(input.position.x) / 8;
    const int y = int(input.position.y) / 8;

#if USE_MSAA
    TEXEL_TYPE dstColor = rtTextureMS.Load(int2(x, y), 0);
#else
    TEXEL_TYPE dstColor = rtTexture.Load(int3(x, y, 0));
#endif

#if USE_DEPTH_STENCIL
    dstColor = clamp((1.0f - dstColor) * 10.0f, 0.0f, 1.0f);
    return float4(dstColor, 0.1f, 1.0f - dstColor, 1.0f);
#else
    return dstColor;
#endif
}

