struct PSInput
{
    float4 position : SV_POSITION;
    nointerpolation float4 color : COLOR;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}

