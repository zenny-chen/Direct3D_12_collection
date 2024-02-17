struct GSOutType
{
    float4 position : SV_POSITION;
    nointerpolation float4 color : COLOR;
    uint viewportIndex : SV_ViewportArrayIndex;
};

float4 PSMain(GSOutType input) : SV_TARGET
{
    return input.color;
}

