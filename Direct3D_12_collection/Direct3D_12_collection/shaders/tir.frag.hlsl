
struct PSInput
{
    centroid float4 position : SV_POSITION;
    centroid float4 color : COLOR;
};

RWBuffer<uint> uavOutput : register(u0, space0);

float4 PSMain(PSInput input, in uint inputCoverage : SV_Coverage, out uint outputCoverage : SV_Coverage) : SV_TARGET
{
    InterlockedAdd(uavOutput[0], 1U);
    
    float4 dstColor = input.color;

    const float inputAlpha = dstColor.a;
    switch (inputCoverage)
    {
    case 0x01U:
        // earthy yellow
        dstColor = float4(0.93f, 0.74f, 0.396f, inputAlpha);
        break;
    case 0x02U:
        // orange
        dstColor = float4(0.975f, 0.5f, 0.025f, inputAlpha);
        break;
    case 0x03U:
        // coffee
        dstColor = float4(0.376f, 0.224f, 0.07f, inputAlpha);
        break;
    case 0x4U:
        // blue
        dstColor = float4(0.1f, 0.1f, 0.9f, inputAlpha);
        break;
    case 0x05U:
        // brown
        dstColor = float4(0.588f, 0.294f, 0.025f, inputAlpha);
        break;
    case 0x06U:
        // cyan
        dstColor = float4(0.1f, 0.9f, 0.9f, inputAlpha);
        break;
    case 0x07U:
        // gray
        dstColor = float4(0.5f, 0.5f, 0.5f, inputAlpha);
        break;
    case 0x08U:
        // black
        dstColor = float4(0.1f, 0.1f, 0.1f, inputAlpha);
        break;
    case 0x09U:
        // magenta
        dstColor = float4(0.9f, 0.1f, 0.9f, inputAlpha);
        break;
    case 0x0aU:
        // green
        dstColor = float4(0.1f, 0.9f, 0.1f, inputAlpha);
        break;
    case 0x0bU:
        // pink
        dstColor = float4(0.975f, 0.753f, 0.025f, inputAlpha);
        break;
    case 0x0cU:
        // purple
        dstColor = float4(0.5f, 0.025f, 0.5f, inputAlpha);
        break;
    case 0x0dU:
        // yellow
        dstColor = float4(0.9f, 0.9f, 0.1f, inputAlpha);
        break;
    case 0x0eU:
        // red
        dstColor = float4(0.9f, 0.1f, 0.1f, inputAlpha);
        break;
    case 0x0fU:
    default:
        break;
    }

    uint dstCoverage = reversebits(inputCoverage);
    outputCoverage = (dstCoverage >> 28) | (0x0fU * 1U);

    return dstColor;
}

