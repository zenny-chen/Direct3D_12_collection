// Direct3D_12_collection.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "common.h"

// Only used in RenderPostProcessForTransformFeedback
static ID3D12Resource* s_readbackBuffer = nullptr;

static auto CreateRootSignature(ID3D12Device* d3d_device) -> ID3D12RootSignature*
{
    ID3D12RootSignature* rootSignature = nullptr;

    const D3D12_DESCRIPTOR_RANGE descRanges[]{
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        },
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        }
    };

    const D3D12_ROOT_PARAMETER rootParameters[]{
        {
            // constant buffer view (CBV)
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[0]
            },
            // This constant buffer will just be accessed in a vertex shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
        },
        {
            // unordered access buffer view (UAV)
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[1]
            },
            // This constant buffer will just be accessed in a vertex shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
        }
    };

    // Create a root signature.
    const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {
        .NumParameters = UINT(std::size(rootParameters)),
        .pParameters = rootParameters,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS
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

static auto CreatePipelineStateObject(ID3D12Device* d3d_device, ID3D12CommandAllocator *commandAllocator, ID3D12CommandAllocator* commandBundleAllocator, ID3D12RootSignature* rootSignature) ->
                                        std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;
    ID3D12DescriptorHeap* descriptorHeap = nullptr;

    auto result = std::make_tuple(pipelineState, commandList, commandBundleList, descriptorHeap);

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("shaders/tfb_basic.vert.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("shaders/basic.frag.cso");

    do
    {
        if (vertexShaderObj.pShaderBytecode == nullptr || vertexShaderObj.BytecodeLength == 0) break;
        if (pixelShaderObj.pShaderBytecode == nullptr || pixelShaderObj.BytecodeLength == 0) break;

        // Define the vertex input layout used for Input Assembler
        const D3D12_INPUT_ELEMENT_DESC inputElementDescs[]{
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{
            .pRootSignature = rootSignature,
            .VS = vertexShaderObj,
            .PS = pixelShaderObj,
            .BlendState {
                .AlphaToCoverageEnable = FALSE,
                .IndependentBlendEnable = FALSE,
                .RenderTarget {
                    // RenderTarget[0]
                    {
                        .BlendEnable = FALSE,
                        .LogicOpEnable = FALSE,
                        .SrcBlend = D3D12_BLEND_SRC_ALPHA,
                        .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
                        .BlendOp = D3D12_BLEND_OP_ADD,
                        .SrcBlendAlpha = D3D12_BLEND_ONE,
                        .DestBlendAlpha = D3D12_BLEND_ZERO,
                        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                        .LogicOp = D3D12_LOGIC_OP_NOOP,
                        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                    }
                }
            },
            .SampleMask = UINT32_MAX,
            // Use the default rasterizer state
            .RasterizerState {
                .FillMode = D3D12_FILL_MODE_SOLID,
                .CullMode = D3D12_CULL_MODE_BACK,
                .FrontCounterClockwise = FALSE,
                .DepthBias = 0,
                .DepthBiasClamp = 0.0f,
                .SlopeScaledDepthBias = 0.0f,
                .DepthClipEnable = TRUE,
                .MultisampleEnable = FALSE,
                .AntialiasedLineEnable = FALSE,
                .ForcedSampleCount = 0,
                .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
            },
            .DepthStencilState {
                .DepthEnable = FALSE,
                .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
                .DepthFunc = D3D12_COMPARISON_FUNC_NEVER,
                .StencilEnable = FALSE,
                .StencilReadMask = 0,
                .StencilWriteMask = 0,
                .FrontFace {},
                .BackFace { }
            },
            .InputLayout {
                .pInputElementDescs = inputElementDescs,
                .NumElements = (UINT)std::size(inputElementDescs)
            },
            .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1,
            .RTVFormats {
                // RTVFormats[0]
                { DXGI_FORMAT_R8G8B8A8_UNORM }
            },
            .DSVFormat = DXGI_FORMAT_UNKNOWN,
            .SampleDesc {
                .Count = 1,
                .Quality = 0
            },
            .NodeMask = 0,
            .CachedPSO { },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE
        };

        HRESULT hRes = d3d_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateGraphicsPipelineState for PSO failed: %ld\n", hRes);
            break;
        }

        hRes = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, pipelineState, IID_PPV_ARGS(&commandList));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommandList for basic PSO failed: %ld\n", hRes);
            break;
        }

        hRes = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, commandBundleAllocator, pipelineState, IID_PPV_ARGS(&commandBundleList));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommandList for command bundle failed: %ld\n", hRes);
            break;
        }

        const D3D12_DESCRIPTOR_HEAP_DESC cbv_uavHeapDesc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 2U,   // Transform Feedback Test needs 2 descriptors -- one for CBV and another for UAV
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0
        };
        hRes = d3d_device->CreateDescriptorHeap(&cbv_uavHeapDesc, IID_PPV_ARGS(&descriptorHeap));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateDescriptorHeap for constant buffer view failed: %ld\n", hRes);
            return result;
        }

        result = std::make_tuple(pipelineState, commandList, commandBundleList, descriptorHeap);

    } while (false);

    if (vertexShaderObj.pShaderBytecode != nullptr) {
        free((void*)vertexShaderObj.pShaderBytecode);
    }
    if (pixelShaderObj.pShaderBytecode != nullptr) {
        free((void*)pixelShaderObj.pShaderBytecode);
    }

    return result;
}

// return: std::make_tuple(uploadDevHostBuffer, readbackDevHostBuffer, vertexBuffer, uavBuffer, constantBuffer)
static auto CreateVertexBuffer(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12CommandQueue *commandQueue,
                                ID3D12GraphicsCommandList* commandList, ID3D12GraphicsCommandList* commandBundleList, ID3D12DescriptorHeap *descriptorHeap) ->
                                std::tuple<ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*>
{
    const struct Vertex
    {
        float position[4];
        float color[4];
    } squareVertices[]{
        // Direct3D是以左手作为前面背面顶点排列的依据
        {.position { -0.75f, 0.75f, 0.0f, 1.0f }, .color { 0.9f, 0.1f, 0.1f, 1.0f } },     // top left
        {.position { 0.75f, 0.75f, 0.0f, 1.0f }, .color { 0.9f, 0.9f, 0.1f, 1.0f } },      // top right
        {.position { -0.75f, -0.75f, 0.0f, 1.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },    // bottom left
        {.position { 0.75f, -0.75f, 0.0f, 1.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } }      // bottom right
    };

    const D3D12_HEAP_PROPERTIES defaultHeapProperties{
        .Type = D3D12_HEAP_TYPE_DEFAULT,    // default heap type for device visible memory
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1
    };

    const D3D12_HEAP_PROPERTIES uploadHeapProperties{
        .Type = D3D12_HEAP_TYPE_UPLOAD,     // for host visible memory which is used to upload data from host to device
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1
    };
    const D3D12_HEAP_PROPERTIES readbackHeapProperties{
        .Type = D3D12_HEAP_TYPE_READBACK,   // for host visible memory which is used to read back data from device to host
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1
    };

    const D3D12_RESOURCE_DESC vbResourceDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = sizeof(squareVertices),
        .Height = 1U,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc {.Count = 1U, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };
    const D3D12_RESOURCE_DESC uavResourceDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = sizeof(squareVertices),
        .Height = 1U,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc {.Count = 1U, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
    };

    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* readbackDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* uavBuffer = nullptr;
    ID3D12Resource* constantBuffer = nullptr;

    auto result = std::make_tuple(uploadDevHostBuffer, readbackDevHostBuffer, vertexBuffer, uavBuffer, constantBuffer);

    // Create vertexBuffer on GPU side.
    HRESULT hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vbResourceDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&vertexBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for vertex buffer failed: %ld\n", hRes);
        return result;
    }

    hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavResourceDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&uavBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for unordered access view buffer failed: %ld\n", hRes);
        return result;
    }

    // Create uploadDevHostBuffer with host visible for upload
    hRes = d3d_device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vbResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadDevHostBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for uploadDevHostBuffer failed: %ld\n", hRes);
        return result;
    }

    D3D12_RESOURCE_DESC readbackResourceDesc = uavResourceDesc;
    readbackResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;      // The only difference is the Flag value
    hRes = d3d_device->CreateCommittedResource(&readbackHeapProperties, D3D12_HEAP_FLAG_NONE, &readbackResourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackDevHostBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for readbackDevHostBuffer failed: %ld\n", hRes);
        return result;
    }

    void* hostMemPtr = nullptr;
    const D3D12_RANGE readRange{ 0, 0 };    // We do not intend to read from this resource on the CPU.
    hRes = uploadDevHostBuffer->Map(0, &readRange, &hostMemPtr);
    if (FAILED(hRes))
    {
        fprintf(stderr, "Map vertex buffer failed: %ld\n", hRes);
        return result;
    }

    memcpy(hostMemPtr, squareVertices, sizeof(squareVertices));
    uploadDevHostBuffer->Unmap(0, nullptr);

    WriteToDeviceResourceAndSync(commandList, vertexBuffer, uploadDevHostBuffer, 0U, 0U, sizeof(squareVertices));

    hRes = commandList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close command list failed: %ld\n", hRes);
        return result;
    }

    // Execute the command list to complete the copy operation
    ID3D12CommandList* const ppCommandLists[] = { (ID3D12CommandList*)commandList };
    commandQueue->ExecuteCommandLists((UINT)std::size(ppCommandLists), ppCommandLists);

    // Initialize the vertex buffer view.
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{
        .BufferLocation = vertexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = (uint32_t)sizeof(squareVertices),
        .StrideInBytes = sizeof(squareVertices[0])
    };

    const D3D12_RESOURCE_DESC cbResourceDesc{
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = CONSTANT_BUFFER_ALLOCATION_GRANULARITY,
            .Height = 1U,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc {.Count = 1U, .Quality = 0 },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    // Create constant buffer object
    hRes = d3d_device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &cbResourceDesc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for constant buffer failed: %ld\n", hRes);
        return result;
    }

    // Upload data to constant buffer
    hRes = constantBuffer->Map(0, &readRange, &hostMemPtr);
    if (FAILED(hRes))
    {
        fprintf(stderr, "Map constant buffer failed: %ld\n", hRes);
        return result;
    }

    memset(hostMemPtr, 0, CONSTANT_BUFFER_ALLOCATION_GRANULARITY);
    constantBuffer->Unmap(0, nullptr);

    // Fetch CBV and UAV CPU descriptor handles
    auto const descHandleIncrSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cbvCPUDescHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE uavCPUDescHandle = cbvCPUDescHandle;
    uavCPUDescHandle.ptr += 1U * descHandleIncrSize;

    // Create the constant buffer view
    const D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{
        .BufferLocation = constantBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = CONSTANT_BUFFER_ALLOCATION_GRANULARITY
    };
    d3d_device->CreateConstantBufferView(&cbvDesc, cbvCPUDescHandle);

    // Create the unordered access buffer view
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer {
            .FirstElement = 0,
            .NumElements = UINT(std::size(squareVertices)),
            .StructureByteStride = sizeof(Vertex),
            .CounterOffsetInBytes = 0,
            .Flags = D3D12_BUFFER_UAV_FLAG_NONE
        }
    };
    d3d_device->CreateUnorderedAccessView(uavBuffer, nullptr, &uavDesc, uavCPUDescHandle);

    // Fetch CBV and UAV GPU descriptor handles
    D3D12_GPU_DESCRIPTOR_HANDLE cbvDescHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE uavDescHandle = cbvDescHandle;
    uavDescHandle.ptr += 1U * descHandleIncrSize;

    // Record commands to the command list bundle.
    commandBundleList->SetGraphicsRootSignature(rootSignature);
    ID3D12DescriptorHeap* const descHeaps[]{ descriptorHeap };
    // ATTENTION: SetDescriptorHeaps should be set into command bundle list as well as command list
    commandBundleList->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
    commandBundleList->SetGraphicsRootDescriptorTable(0, cbvDescHandle);        // rootParameters[0]
    commandBundleList->SetGraphicsRootDescriptorTable(1, uavDescHandle);        // rootParameters[1]
    commandBundleList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    commandBundleList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandBundleList->DrawInstanced((UINT)std::size(squareVertices), 1, 0, 0);

    // End of the record
    hRes = commandBundleList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close basic command bundle failed: %ld\n", hRes);
        return result;
    }

    // Wait for the command list to execute;
    // we are reusing the same command list in our main loop but for now,
    // we just want to wait for setup to complete before continuing.
    WaitForPreviousFrame(commandQueue);

    result = std::make_tuple(uploadDevHostBuffer, readbackDevHostBuffer, vertexBuffer, uavBuffer, constantBuffer);
    return result;
}

auto CreateTransformFeedbackTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue *commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                    std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*>
{
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;
    ID3D12DescriptorHeap* descriptorHeap = nullptr;
    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* readbackDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* uavBuffer = nullptr;
    ID3D12Resource* constantBuffer = nullptr;

    auto result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundleList, descriptorHeap, uploadDevHostBuffer, readbackDevHostBuffer, vertexBuffer, uavBuffer, constantBuffer);

    rootSignature = CreateRootSignature(d3d_device);
    if (rootSignature == nullptr) return result;

    auto pipelineResult = CreatePipelineStateObject(d3d_device, commandAllocator, commandBundleAllocator, rootSignature);
    pipelineState = std::get<0>(pipelineResult);
    commandList = std::get<1>(pipelineResult);
    commandBundleList = std::get<2>(pipelineResult);
    descriptorHeap = std::get<3>(pipelineResult);
    if (pipelineState == nullptr || commandList == nullptr || commandBundleList == nullptr || descriptorHeap == nullptr) return result;

    auto vertexBufferResult = CreateVertexBuffer(d3d_device, rootSignature, commandQueue, commandList, commandBundleList, descriptorHeap);
    uploadDevHostBuffer = std::get<0>(vertexBufferResult);
    readbackDevHostBuffer = std::get<1>(vertexBufferResult);
    vertexBuffer = std::get<2>(vertexBufferResult);
    uavBuffer = std::get<3>(vertexBufferResult);
    constantBuffer = std::get<4>(vertexBufferResult);
    if (uploadDevHostBuffer == nullptr || readbackDevHostBuffer == nullptr || vertexBuffer == nullptr || uavBuffer == nullptr || constantBuffer == nullptr) return result;

    s_readbackBuffer = readbackDevHostBuffer;
    result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundleList, descriptorHeap, uploadDevHostBuffer, readbackDevHostBuffer, vertexBuffer, uavBuffer, constantBuffer);
    return result;
}

auto RenderPostProcessForTransformFeedback() -> void
{
    float* hostMemPtr = nullptr;
    const D3D12_RANGE readRange{ 0, 0 };
    HRESULT hRes = s_readbackBuffer->Map(0, &readRange, (void**)&hostMemPtr);
    if (FAILED(hRes))
    {
        fprintf(stderr, "Map read back buffer failed: %ld\n", hRes);
        return;
    }

    s_readbackBuffer->Unmap(0, nullptr);
}

