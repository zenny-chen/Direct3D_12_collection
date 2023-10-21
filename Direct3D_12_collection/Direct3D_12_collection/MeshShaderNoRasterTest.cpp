#include "common.h"

struct MyVertexType
{
    float position[4];
    float color[4];
};

static auto CreateMeshShaderRootSignature(ID3D12Device *d3d_device) -> ID3D12RootSignature*
{
    ID3D12RootSignature* rootSignature = nullptr;

    const D3D12_ROOT_PARAMETER rootParameter{
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV,
        .Descriptor {
            .ShaderRegister = 0,
            .RegisterSpace = 0
        },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_MESH
    };

    // Create a root signature.
    const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {
        .NumParameters = 1,
        .pParameters = &rootParameter,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS
    };

    ID3DBlob* signature = nullptr;
    ID3DBlob* error = nullptr;
    HRESULT hRes = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hRes))
    {
        fprintf(stderr, "D3D12SerializeRootSignature failed: %ld\n", hRes);
        return rootSignature;
    }

    hRes = d3d_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateRootSignature failed: %ld\n", hRes);
        return rootSignature;
    }

    return rootSignature;
}

static auto CreateMeshShaderPipelineStateObject(ID3D12Device2* d3d_device, ID3D12RootSignature* rootSignature,
                                                    ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
            std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*>
{
    const D3D12_SHADER_BYTECODE meshShaderObj = CreateCompiledShaderObjectFromPath("shaders/simple_ms.mesh.cso");

    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;

    std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*> result = std::make_tuple(pipelineState, commandList, commandBundle);

    bool done = false;
    do
    {
        if (meshShaderObj.pShaderBytecode == nullptr || meshShaderObj.BytecodeLength == 0) break;

        // Describe and create the graphics pipeline state object (PSO).
        // The format of the provided stream should consist of an alternating set of D3D12_PIPELINE_STATE_SUBOBJECT_TYPE, 
        // and the corresponding subobject types for them.
        // For example, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER pairs with D3D12_RASTERIZER_DESC.
        struct {
            RootSignatureSubobject rootSignatureSubobject;
            ShaderByteCodeSubobject msShaderSubobject;
            SampleMaskSubobject sampleMaskSubobject;
            IBStripCutValueSubobject ibStripCutValueSubobject;
            PrimitiveTopologyTypeSubobject primitiveTopologySubobject;
            SampleDescSubobject sampleDescSubobject;
            NodeMaskSubobject nodeMaskSubobject;
            CachedPSOSubobject cachedPSOSubobject;
            FlagsSubobject flagsSubobject;
        } psoStream {
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, rootSignature },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, meshShaderObj },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK, UINT32_MAX },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC, {
                        .Count = 1,
                        .Quality = 0
                    }
            },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK, 0 },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO, { } },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS, D3D12_PIPELINE_STATE_FLAG_NONE }
        };

        const D3D12_PIPELINE_STATE_STREAM_DESC streamDesc{
            .SizeInBytes = sizeof(psoStream),
            .pPipelineStateSubobjectStream = &psoStream
        };

        HRESULT hRes = d3d_device->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&pipelineState));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreatePipelineState failed: %ld\n", hRes);
            break;
        }

        hRes = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, pipelineState, IID_PPV_ARGS(&commandList));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommandList for Mesh Shader PSO failed: %ld\n", hRes);
            break;
        }

        hRes = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, commandBundleAllocator, pipelineState, IID_PPV_ARGS(&commandBundle));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommandList for command bundle failed: %ld\n", hRes);
            break;
        }
        
        done = true;
    }
    while (false);

    if (meshShaderObj.pShaderBytecode != nullptr) {
        free((void*)meshShaderObj.pShaderBytecode);
    }

    result = std::make_tuple(pipelineState, commandList, commandBundle);
    return result;
}

// Return std::make_pair(devHostReadBuffer, uavBuffer, uavDescriptorHeap)
static auto CreateUAVBuffer(ID3D12Device* d3d_device) -> std::tuple<ID3D12Resource*, ID3D12Resource*, ID3D12DescriptorHeap*>
{
    const D3D12_HEAP_PROPERTIES defaultHeapProperties{
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1
    };

    const D3D12_HEAP_PROPERTIES readHeapProperties{
        .Type = D3D12_HEAP_TYPE_READBACK,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1
    };

    const D3D12_RESOURCE_DESC uavResourceDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = UINT64(sizeof(MyVertexType) * 4U),
        .Height = 1U,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc {.Count = 1U, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
    };

    const D3D12_RESOURCE_DESC readResourceDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = UINT64(sizeof(MyVertexType) * 4U),
        .Height = 1U,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc {.Count = 1U, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    ID3D12Resource* devHostReadBuffer = nullptr;
    ID3D12Resource* uavBuffer = nullptr;
    ID3D12DescriptorHeap* uavDescriptorHeap = nullptr;

    auto result = std::make_tuple(devHostReadBuffer, uavBuffer, uavDescriptorHeap);

    // Create uavBuffer on GPU side.
    HRESULT hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavResourceDesc,
                                                    D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&uavBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for UAV buffer failed: %ld\n", hRes);
        return result;
    }

    // Create devHostReadBuffer with host visible for readback
    hRes = d3d_device->CreateCommittedResource(&readHeapProperties, D3D12_HEAP_FLAG_NONE, &readResourceDesc,
                                            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&devHostReadBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for constant buffer failed: %ld\n", hRes);
        return result;
    }

    // ---- Create descriptor heaps. ----
    const D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0
    };
    hRes = d3d_device->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(&uavDescriptorHeap));

    // Create the UAV buffer view
    const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer {
            .FirstElement = 0,
            .NumElements = 4U,
            .StructureByteStride = sizeof(MyVertexType),
            .CounterOffsetInBytes = 0,
            .Flags = D3D12_BUFFER_UAV_FLAG_NONE
        }
    };

    d3d_device->CreateUnorderedAccessView(uavBuffer, nullptr, &uavDesc, uavDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    result = std::make_tuple(devHostReadBuffer, uavBuffer, uavDescriptorHeap);
    return result;
}

static auto PopulateMeshShaderCommandBundleList(ID3D12RootSignature *rootSignature, ID3D12CommandQueue *commandQueue,
                                            ID3D12GraphicsCommandList *commandList, ID3D12GraphicsCommandList6 *commandBundleList,
                                            ID3D12Resource* uavBuffer, ID3D12Resource* hostReadbackBuffer) -> bool
{
    // Record commands to the command list bundle.
    commandBundleList->SetGraphicsRootSignature(rootSignature);
    commandBundleList->SetGraphicsRootUnorderedAccessView(0, uavBuffer->GetGPUVirtualAddress());
    commandBundleList->DispatchMesh(4U, 1U, 1U);
    
    // End of the record
    HRESULT hRes = commandBundleList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close Mesh Shader command bundle failed: %ld\n", hRes);
        return false;
    }

    commandList->ExecuteBundle(commandBundleList);

    SyncAndReadFromDeviceResource(commandList, sizeof(MyVertexType) * 4U, hostReadbackBuffer, uavBuffer);

    hRes = commandList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close Mesh Shader command list failed: %ld\n", hRes);
        return false;
    }

    ID3D12CommandList* const ppCommandLists[] = { (ID3D12CommandList*)commandList };
    commandQueue->ExecuteCommandLists((UINT)std::size(ppCommandLists), ppCommandLists);

    if (!WaitForPreviousFrame(commandQueue)) return false;

    float* hostMemPtr = nullptr;
    const D3D12_RANGE readRange{ 0, 0 };    // We do not intend to read from this resource on the CPU.
    hRes = hostReadbackBuffer->Map(0, &readRange, (void**)&hostMemPtr);
    if (FAILED(hRes))
    {
        fprintf(stderr, "Map vertex buffer failed: %ld\n", hRes);
        return false;
    }

    puts("Vertices and colors: ");

    for (int i = 0, index = 0; i < 4; ++i)
    {
        const float v0 = hostMemPtr[index++];
        const float v1 = hostMemPtr[index++];
        const float v2 = hostMemPtr[index++];
        const float v3 = hostMemPtr[index++];

        const float c0 = hostMemPtr[index++];
        const float c1 = hostMemPtr[index++];
        const float c2 = hostMemPtr[index++];
        const float c3 = hostMemPtr[index++];

        printf("(%.1f, %.1f, %.1f, %.1f); (%.1f, %.1f, %.1f, %.1f)\n",
            v0, v1, v2, v3, c0, c1, c2, c3);
    }

    hostReadbackBuffer->Unmap(0, nullptr);

    return true;
}

auto CreateMeshShaderNoRasterTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*, ID3D12DescriptorHeap*>
{
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;
    ID3D12Resource* devHostBuffer = nullptr;
    ID3D12Resource* unorderedAccessBuffer = nullptr;
    ID3D12DescriptorHeap* descHeap = nullptr;

    auto result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundleList, devHostBuffer, unorderedAccessBuffer, descHeap);

    rootSignature = CreateMeshShaderRootSignature(d3d_device);
    if (rootSignature == nullptr) return result;

    auto pipelineRes = CreateMeshShaderPipelineStateObject((ID3D12Device2*)d3d_device, rootSignature, commandAllocator, commandBundleAllocator);
    pipelineState = std::get<0>(pipelineRes);
    commandList = std::get<1>(pipelineRes);
    commandBundleList = std::get<2>(pipelineRes);

    auto uavResult = CreateUAVBuffer(d3d_device);
    devHostBuffer = std::get<0>(uavResult);
    unorderedAccessBuffer = std::get<1>(uavResult);
    descHeap = std::get<2>(uavResult);

    if (!PopulateMeshShaderCommandBundleList(rootSignature, commandQueue, commandList, (ID3D12GraphicsCommandList6*)commandBundleList, unorderedAccessBuffer, devHostBuffer)) return result;

    result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundleList, devHostBuffer, unorderedAccessBuffer, descHeap);
    return result;
}

