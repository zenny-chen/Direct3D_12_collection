struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoords : TEXCOORD;
};

Texture2D<float4> texture : register(t0, space0);
SamplerState texSampler : register(s0, space0);

float4 PSMain(PSInput input) : SV_TARGET
{
    return texture.Sample(texSampler, input.texCoords);
}

