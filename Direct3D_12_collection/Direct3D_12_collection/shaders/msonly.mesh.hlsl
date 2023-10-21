
struct MyOutputVertex
{
    float4 vertCoord : SV_Position;
    float4 color : COLOR;
};

struct CBColorVarying
{
    float4 bottomLeft;
    float4 topLeft;
    float4 bottomRight;
    float4 topRight;
};

ConstantBuffer<CBColorVarying> cbColorVaryings : register(b0);

// max vertex count should not exceed 256
// max primitive count should not exceed 256
[outputtopology("triangle")]        // equivalent to [outputtopology("triangle_cw")]
[numthreads(128, 1, 1)]
void MeshMain(in uint globalTID : SV_DispatchThreadID, in uint3 groupID : SV_GroupID, in uint3 localTID : SV_GroupThreadID,
            out vertices MyOutputVertex outVertBuffer[256], out indices uint3 outPrimIndices[254])
{
    // We're going to generate 256 vertices and 254 triangles (127 differential rectangles)
    SetMeshOutputCounts(256, 254);

    const float edgeLength = 0.5f;
    const float baseCoord = edgeLength * 0.5f;
    const float dx = edgeLength / 127.0f;
    const float step = float(localTID.x);

    // podPattern == 0
    const float x = -baseCoord + dx * step;

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

    const float4 dcBottom = (cbColorVaryings.bottomRight - cbColorVaryings.bottomLeft) / 127.0f;
    const float4 dcTop = (cbColorVaryings.topRight - cbColorVaryings.topLeft) / 127.0f;

    float4 color0 = cbColorVaryings.bottomLeft + dcBottom * step;
    float4 color1 = cbColorVaryings.topLeft + dcTop * step;
    color0.a = 1.0f;
    color1.a = 1.0f;

    outVertBuffer[localTID.x * 2U + 0U].color = color0;
    outVertBuffer[localTID.x * 2U + 1U].color = color1;

    // Assemble the primitive
    if (localTID.x >= 127) return;

    // Each work item assembles 2 primitives (2 triangles compose 1 rectangle)
    const uint v0 = localTID.x * 2;
    const uint v1 = v0 + 1U;
    const uint v2 = v0 + 2U;
    const uint v3 = v0 + 3U;

    outPrimIndices[localTID.x * 2 + 0U] = uint3(v0, v1, v2);
    outPrimIndices[localTID.x * 2 + 1U] = uint3(v2, v1, v3);
}

