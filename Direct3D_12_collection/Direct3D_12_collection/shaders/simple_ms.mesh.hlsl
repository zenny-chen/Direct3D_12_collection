
struct MyVertexType
{
    float4 vertCoord;
    float4 color;
};

RWStructuredBuffer<MyVertexType> uavOutput : register(u0);

// max vertex count should not exceed 256
// max primitive count should not exceed 256
[outputtopology("triangle")]        // equivalent to [outputtopology("triangle_cw")]
[numthreads(128, 1, 1)]
void MeshMain(in uint globalTID : SV_DispatchThreadID, in uint3 groupID : SV_GroupID, in uint3 localTID : SV_GroupThreadID
            // , out vertices MyOutputVertex outVertBuffer[4], out indices uint3 outPrimIndices[2]
            )
{
    // We're going to generate 4 vertices and 2 triangles
    SetMeshOutputCounts(4, 2);

    const float xOffsetList[4] = { -0.5f, 0.5f, -0.5f, 0.5f };
    const float yOffsetList[4] = { 0.5f, 0.5f, -0.5f, -0.5f };

    const float4 vert0 = float4(-0.5f + xOffsetList[groupID.x], -0.5f + yOffsetList[groupID.x], 0.0f, 1.0f);
    const float4 vert1 = float4(-0.5f + xOffsetList[groupID.x], 0.5f + yOffsetList[groupID.x], 0.0f, 1.0f);
    const float4 vert2 = float4(0.5f + xOffsetList[groupID.x], -0.5f + yOffsetList[groupID.x], 0.0f, 1.0f);
    const float4 vert3 = float4(0.5f + xOffsetList[groupID.x], 0.5f + yOffsetList[groupID.x], 0.0f, 1.0f);

    const uint index = groupID.x * 4U;
    uavOutput[index + 0].vertCoord = vert0;
    uavOutput[index + 1].vertCoord = vert1;
    uavOutput[index + 2].vertCoord = vert2;
    uavOutput[index + 3].vertCoord = vert3;

    uavOutput[index + 0].color = float4(1.0f, 0.0f, 0.0f, 1.0f);
    uavOutput[index + 1].color = float4(0.0f, 1.0f, 0.0f, 1.0f);
    uavOutput[index + 2].color = float4(0.0f, 0.0f, 1.0f, 1.0f);
    uavOutput[index + 3].color = float4(1.0f, 1.0f, 0.0f, 1.0f);
}

