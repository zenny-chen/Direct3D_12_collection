struct MyPayloadType
{
    int data[1024];
    uint meshID;
    uint meshGroupSize;
};

groupshared MyPayloadType sharedPayload;

[numthreads(128, 1, 1)]
void AmplificationMain(in uint3 groupID : SV_GroupID, in uint3 localTID : SV_GroupThreadID)
{
    if (true)
    {
        MyPayloadType payload;
        const int baseData = int(groupID.x * groupID.y * groupID.z);

        for (int i = 0; i < 1024; ++i) {
            payload.data[i] = baseData + i;
        }
        payload.meshID = groupID.x;
        payload.meshGroupSize = 4;      // launch 4 mesh shader groups

        DispatchMesh(payload.meshGroupSize, 1, 1, payload);
    }
    else
    {
        sharedPayload.data[localTID.x * 8U + 0] = int(groupID.x);
        sharedPayload.data[localTID.x * 8U + 1] = int(groupID.y);
        sharedPayload.data[localTID.x * 8U + 2] = int(groupID.z);
        sharedPayload.data[localTID.x * 8U + 3] = int(localTID.x);
        sharedPayload.data[localTID.x * 8U + 4] = int(localTID.y);
        sharedPayload.data[localTID.x * 8U + 5] = int(localTID.z);
        sharedPayload.data[localTID.x * 8U + 6] = 0;
        sharedPayload.data[localTID.x * 8U + 7] = 1;

        GroupMemoryBarrierWithGroupSync();

        DispatchMesh(4, 1, 1, sharedPayload);
    }
}

