
// Fused with Draw Arguments, Draw Indexed Arguments and Dispatch Arguments
// sizeof(IndirectArgumentBufferType) MUST BE at least 32 bytes.
struct IndirectArgumentBufferType
{
    uint VertexCountPerInstance_IndexCountPerInstance_ThreadGroupX;
    uint InstanceCount_ThreadGroupCountY;
    uint StartVertexLocation_StartIndexLocation_ThreadGroupCountZ;
    uint StartInstanceLocation_BaseVertexLocation;
    uint StartInstanceLocation;
    uint paddings[3];
};

RWStructuredBuffer<IndirectArgumentBufferType> uavIndirectArgBuffer : register(u0, space0);

//RWBuffer<uint> uavCountBuffer : register(u1, space0);
// Intel HD Graphics and Iris Pro Graphics DO NOT support RWBuffer.
RWStructuredBuffer<uint> uavCountBuffer : register(u1, space0);

[numthreads(128, 1, 1)]
void CSMain(in uint3 threaID : SV_DispatchThreadID)
{
    const uint index = threaID.x;
    if (index != 0U) return;

    // 4 draw commands
    for (uint i = 0U; i < 4U; ++i)
    {
        // vertex count per instance
        uavIndirectArgBuffer[i].VertexCountPerInstance_IndexCountPerInstance_ThreadGroupX = 4U;
        // instance count
        uavIndirectArgBuffer[i].InstanceCount_ThreadGroupCountY = 1U;
        // start vertex location
        uavIndirectArgBuffer[i].StartVertexLocation_StartIndexLocation_ThreadGroupCountZ = i * 4U;
        // start instance location
        uavIndirectArgBuffer[i].StartInstanceLocation_BaseVertexLocation = 0U;
    }

    // 1 draw index command
    // index count per instance
    uavIndirectArgBuffer[4].VertexCountPerInstance_IndexCountPerInstance_ThreadGroupX = 360 * 2U;
    // instance count
    uavIndirectArgBuffer[4].InstanceCount_ThreadGroupCountY = 1U;
    // start index location
    uavIndirectArgBuffer[4].StartVertexLocation_StartIndexLocation_ThreadGroupCountZ = 0U;
    // base vertex location
    uavIndirectArgBuffer[4].StartInstanceLocation_BaseVertexLocation = 0U;
    // start instance location
    uavIndirectArgBuffer[4].StartInstanceLocation = 0U;

    // 1 dispatch mesh command
    // thread group X
    uavIndirectArgBuffer[5].VertexCountPerInstance_IndexCountPerInstance_ThreadGroupX = 1U;
    // thread group Y
    uavIndirectArgBuffer[5].InstanceCount_ThreadGroupCountY = 1U;
    // thread group Z
    uavIndirectArgBuffer[5].StartVertexLocation_StartIndexLocation_ThreadGroupCountZ = 1U;

    uavCountBuffer[0] = 4U;
    uavCountBuffer[1] = 1U;
    uavCountBuffer[2] = 1U;
}

