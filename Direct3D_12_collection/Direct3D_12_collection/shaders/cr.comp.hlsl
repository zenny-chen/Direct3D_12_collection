#define USE_MSAA                1

#if USE_MSAA
Texture2DMS<float> depthTextureMS : register(t0, space0);
#else
Texture2D<float> depthTexture : register(t0, space0);
#endif

RWBuffer<float> uavOutput : register(u0, space0);

struct CBSampleIndex
{
    int sampeIndex;
};

ConstantBuffer<CBSampleIndex> cbSampleIndex : register(b0, space0);

[numthreads(16, 16, 1)]
void CSMain(in uint3 threaID : SV_DispatchThreadID)
{
    const uint x = threaID.x;
    const uint y = threaID.y;
    const uint index = y * 64 + x;

#if USE_MSAA
    const float depthValue = depthTextureMS.Load(int2(x, y), cbSampleIndex.sampeIndex);
#else
    const float depthValue = depthTexture.Load(int3(x, y, 0));
#endif

    uavOutput[index] = depthValue;
}

