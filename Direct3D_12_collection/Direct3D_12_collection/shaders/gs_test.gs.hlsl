struct VSOut
{
    float4 position : POSITION;
    nointerpolation float4 color : COLOR;
};

struct GSOutType
{
    float4 position : SV_POSITION;      // Geometry Shader output position attribute should be qualified with `SV_POSITION`
    nointerpolation float4 color : COLOR;
    uint viewportIndex : SV_ViewportArrayIndex;
    uint primID : SV_PrimitiveID;
};

[maxvertexcount(16)]
// The input primitives have `point` primitive types.
void GSMain(in point VSOut gsIn[1], in uint primIDIn : SV_PrimitiveID, inout TriangleStream<GSOutType> gsOutStream)
{
    float4 colorList[4];
    colorList[0] = gsIn[0].color;

    switch (primIDIn)
    {
    case 0U:
    default:
        colorList[1] = float4(1.0f - colorList[0].r, 1.0f - colorList[0].g, colorList[0].b, colorList[0].a);
        colorList[2] = float4(1.0f - colorList[0].r, colorList[0].g, 1.0f - colorList[0].b, colorList[0].a);
        colorList[3] = float4(colorList[0].r, 1.0f - colorList[0].g, colorList[0].b, colorList[0].a);
        break;

    case 1U:
        colorList[1] = float4(1.0f - colorList[0].r, 1.0f - colorList[0].g, colorList[0].b, colorList[0].a);
        colorList[2] = float4(colorList[0].r, 1.0f - colorList[0].g, 1.0f - colorList[0].b, colorList[0].a);
        colorList[3] = float4(1.0f - colorList[0].r, colorList[0].g, colorList[0].b, colorList[0].a);
        break;

    case 2U:
        colorList[1] = float4(1.0f - colorList[0].r, colorList[0].g, 1.0f - colorList[0].b, colorList[0].a);
        colorList[2] = float4(colorList[0].r, 1.0f - colorList[0].g, 1.0f - colorList[0].b, colorList[0].a);
        colorList[3] = float4(1.0f - colorList[0].r, colorList[0].g, colorList[0].b, colorList[0].a);
        break;

    case 3U:
        colorList[1] = float4(1.0f - colorList[0].r, colorList[0].g, colorList[0].b, colorList[0].a);
        colorList[2] = float4(colorList[0].r, 1.0f - colorList[0].g, colorList[0].b, colorList[0].a);
        colorList[3] = float4(1.0f - colorList[0].r, colorList[0].g, 1.0f - colorList[0].b, colorList[0].a);
        break;
    }
    
    // first top-left square
    float xPos = -0.8f;
    float yPos = 0.1f;
    const float width = 0.7f;

    for (uint primIndex = 0U; primIndex < 4U; ++primIndex)
    {
        GSOutType outPrimObj;

        // bottom-left 
        outPrimObj.position = float4(xPos, yPos, gsIn[0].position.z, gsIn[0].position.w);
        outPrimObj.color = colorList[primIndex];
        outPrimObj.viewportIndex = primIDIn;
        outPrimObj.primID = primIDIn;
        gsOutStream.Append(outPrimObj);

        // top-left
        outPrimObj.position = float4(xPos, yPos + width, gsIn[0].position.z, gsIn[0].position.w);
        outPrimObj.color = colorList[primIndex];
        outPrimObj.viewportIndex = primIDIn;
        outPrimObj.primID = primIDIn;
        gsOutStream.Append(outPrimObj);

        // bottom-right
        outPrimObj.position = float4(xPos + width, yPos, gsIn[0].position.z, gsIn[0].position.w);
        outPrimObj.color = colorList[primIndex];
        outPrimObj.viewportIndex = primIDIn;
        outPrimObj.primID = primIDIn;
        gsOutStream.Append(outPrimObj);

        // top-right
        outPrimObj.position = float4(xPos + width, yPos + width, gsIn[0].position.z, gsIn[0].position.w);
        outPrimObj.color = colorList[primIndex];
        outPrimObj.viewportIndex = primIDIn;
        outPrimObj.primID = primIDIn;
        gsOutStream.Append(outPrimObj);

        gsOutStream.RestartStrip();

        switch (primIndex)
        {
        case 0U:
            // top-right square
            xPos = 0.1f;
            break;

        case 1U:
            // bottom-left square
            xPos = -0.8f;
            yPos = -0.8f;
            break;

        case 2U:
            // bottom-right sqaure
            xPos = 0.1f;
            break;

        default:
            break;
        }
    }
}

