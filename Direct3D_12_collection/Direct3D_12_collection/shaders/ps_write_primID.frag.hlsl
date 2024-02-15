struct PSInput
{
    float4 position : SV_POSITION;
};

float4 PSMain(PSInput input, in uint inputPrimitiveID : SV_PrimitiveID
    // invalid ps_5_1 output semantic 'SV_PrimitiveID'
    //, out uint outputPrimitiveID : SV_PrimitiveID
            ) : SV_TARGET
{
    float4 color = float4(0.1f, 0.1f, 0.1f, 1.0f);
    switch (inputPrimitiveID)
    {
    case 0U:
        color = float4(0.9f, 0.1f, 0.1f, 1.0f);
        break;

    case 1U:
        color = float4(0.1f, 0.9f, 0.1f, 1.0f);
        break;

    case 2U:
        color = float4(0.1f, 0.1f, 0.9f, 1.0f);
        break;

    case 3U:
        color = float4(0.9f, 0.9f, 0.1f, 1.0f);
        break;

    default:
        break;
    }

    return color;
}

