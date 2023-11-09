struct MyPayloadType
{
    int data[1024];
    uint meshID;
    uint meshGroupSize;
};

struct MyOutputVertex
{
    float4 vertCoord : SV_Position;
    float4 color : COLOR;
};

struct MyOutputPrimitive
{
    uint shadingRate : SV_ShadingRate;
};

// max vertex count should not exceed 256
// max primitive count should not exceed 256
[outputtopology("triangle")]        // equivalent to [outputtopology("triangle_cw")]
[numthreads(128, 1, 1)]
void MeshMain(in uint globalTID : SV_DispatchThreadID, in uint3 groupID : SV_GroupID, in uint3 localTID : SV_GroupThreadID,
            in payload MyPayloadType inPayload, out vertices MyOutputVertex outVertBuffer[256], out indices uint3 outPrimIndices[254], out primitives MyOutputPrimitive outPrims[254])
{
    // We're going to generate 256 vertices and 254 triangles (127 differential rectangles)
    SetMeshOutputCounts(256, 254);

    const float edgeLength = 0.5f;
    const float baseCoord = edgeLength * 0.5f;
    const float dx = edgeLength / 127.0f;

    // podPattern == 0
    const float x = -baseCoord + dx * float(localTID.x);

    const float4 vert0 = float4(x, -baseCoord, 0.0f, 1.0f);
    const float4 vert1 = float4(x, baseCoord, 0.0f, 1.0f);

    const float xOffsetList[4] = { -0.5f, 0.5f, -0.5f, 0.5f };
    const float yOffsetList[4] = { 0.5f, 0.5f, -0.5f, -0.5f };

    // glTranslate(xOffset, yOffset, -2.3, 1.0)
    const float4x4 translateMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,     // row 0
        0.0f, 1.0f, 0.0f, 0.0f,     // row 1
        0.0f, 0.0f, 1.0f, 0.0f,     // row 2
        xOffsetList[groupID.x], yOffsetList[groupID.x], -2.3f, 1.0f     // row 3
    };

    // glOrtho(-1.0, 1.0, -1.0, 1.0, 1.0, 3.0)
    const float4x4 projectionMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,     // row 0
        0.0f, 1.0f, 0.0f, 0.0f,     // row 1
        0.0f, 0.0f, -1.0f, 0.0f,    // row 2
        0.0f, 0.0f, -2.0f, 1.0f     // row 3
    };

    const float4x4 mvpMatrix = mul(translateMatrix, projectionMatrix);

    // Each work item generates 2 vertices
    outVertBuffer[localTID.x * 2U + 0U].vertCoord = mul(vert0, mvpMatrix);
    outVertBuffer[localTID.x * 2U + 1U].vertCoord = mul(vert1, mvpMatrix);

    float c0 = float(inPayload.data[groupID.x * 256 + localTID.x]) / 256.0f;
    float c1 = float(inPayload.data[groupID.x * 256 + 128 + localTID.x]) / 256.0f;
    
    c0 = frac(c0);
    float rc0 = 1.0f - c0;
    c1 = frac(c1);
    float rc1 = 1.0f - c1;

    c0 = clamp(c0, 0.1f, 0.9f);
    rc0 = clamp(rc0, 0.1f, 0.9f);
    c1 = clamp(c1, 0.1f, 0.9f);
    rc1 = clamp(rc1, 0.1f, 0.9f);

    outVertBuffer[localTID.x * 2U + 0U].color = float4(c0, c0, rc0, 1.0f);
    outVertBuffer[localTID.x * 2U + 1U].color = float4(c1, c1, rc1, 1.0f);

    // Assemble the primitive
    if (localTID.x >= 127) return;

    // Each work item assembles 2 primitives (2 triangles compose 1 rectangle)
    const uint v0 = localTID.x * 2;
    const uint v1 = v0 + 1U;
    const uint v2 = v0 + 2U;
    const uint v3 = v0 + 3U;

    outPrimIndices[localTID.x * 2 + 0U] = uint3(v0, v1, v2);
    outPrimIndices[localTID.x * 2 + 1U] = uint3(v2, v1, v3);

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
    // Each vertex in a primitve should have the same shading rate size
    outPrims[localTID.x * 2U + 0U].shadingRate = D3D12_SHADING_RATE_1X1;
    outPrims[localTID.x * 2U + 1U].shadingRate = D3D12_SHADING_RATE_1X1;
}

