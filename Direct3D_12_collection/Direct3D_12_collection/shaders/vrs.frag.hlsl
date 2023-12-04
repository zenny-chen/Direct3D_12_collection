struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 PSMain(PSInput input, uint shadingRate : SV_ShadingRate, uint sampleIndex : SV_SampleIndex) : SV_TARGET
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

    float4 outColor = input.color;

    switch (shadingRate)
    {
    case D3D12_SHADING_RATE_1X1:
    default:
        break;

    case D3D12_SHADING_RATE_1X2:
        // red
        outColor = float4(0.9f, 0.1f, 0.1f, 1.0f);
        break;

    case D3D12_SHADING_RATE_2X1:
        // green
        outColor = float4(0.1f, 0.9f, 0.1f, 1.0f);
        break;

    case D3D12_SHADING_RATE_2X2:
        // yellow
        outColor = float4(0.9f, 0.9f, 0.1f, 1.0f);
        break;

    case D3D12_SHADING_RATE_2X4:
        // blue
        outColor = float4(0.1f, 0.1f, 0.9f, 1.0f);
        break;

    case D3D12_SHADING_RATE_4X2:
        // cyan
        outColor = float4(0.1f, 0.9f, 0.9f, 1.0f);
        break;

    case D3D12_SHADING_RATE_4X4:
        // magenta
        outColor = float4(0.9f, 0.1f, 0.9f, 1.0f);
        break;
    }

    return outColor;
}

