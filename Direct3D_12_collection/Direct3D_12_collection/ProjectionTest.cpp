#include "common.h"


constexpr UINT UAV_BUFFER_SIZE = 16U;

static float s_currX = 0.0f;
static float s_currY = 0.0f;
static float s_currZ = -6.0f;   // For orthogonal projection, the proper range is [-6, -8], and [-3, -9] for frustum perspective projection 
static float s_currAngle = 0.0f;

static auto CreateRootSignature(ID3D12Device* d3d_device, bool isForRenderTexture) -> ID3D12RootSignature*
{
    ID3D12RootSignature* rootSignature = nullptr;

    const D3D12_ROOT_PARAMETER rootParameters[] {
        // b0
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor {
                .ShaderRegister = 0,
                .RegisterSpace = 0
            },
            // This unordered access view buffer will just be accessed in a pixel shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
        }
    };

    // Create a root signature.
    const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {
        .NumParameters = (UINT)std::size(rootParameters),
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
    do
    {
        if (FAILED(hRes))
        {
            fprintf(stderr, "D3D12SerializeRootSignature failed: %ld\n", hRes);
            break;
        }

        hRes = d3d_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateRootSignature failed: %ld\n", hRes);
            break;
        }
    }
    while (false);

    if (signature != nullptr) {
        signature->Release();
    }
    if (error != nullptr) {
        error->Release();
    }

    if (FAILED(hRes)) return nullptr;

    return rootSignature;
}

// @return [pipelineState, commandList, commandBundleList]
static auto CreatePipelineStateObject(ID3D12Device* d3d_device, ID3D12CommandAllocator *commandAllocator, ID3D12CommandAllocator* commandBundleAllocator, ID3D12RootSignature* rootSignature) ->
                                        std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;

    auto result = std::make_tuple(pipelineState, commandList, commandBundleList);

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("shaders/proj_test.vs.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("shaders/proj_test.ps.cso");

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
                .CullMode = D3D12_CULL_MODE_NONE,
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
                .FrontFace { },
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
                { RENDER_TARGET_BUFFER_FOMRAT }
            },
            .DSVFormat = DXGI_FORMAT_D32_FLOAT,
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

        result = std::make_tuple(pipelineState, commandList, commandBundleList);
    }
    while (false);

    if (vertexShaderObj.pShaderBytecode != nullptr) {
        free((void*)vertexShaderObj.pShaderBytecode);
    }
    if (pixelShaderObj.pShaderBytecode != nullptr) {
        free((void*)pixelShaderObj.pShaderBytecode);
    }

    return result;
}

// @return [uploadDevHostBuffer, vertexBuffer, constantBuffer]
static auto CreateVertexBuffer(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12CommandQueue *commandQueue, ID3D12GraphicsCommandList* commandList,
                                            ID3D12GraphicsCommandList* commandBundleList) -> std::tuple<ID3D12Resource*, ID3D12Resource*, ID3D12Resource*>
{
    const struct Vertex
    {
        float position[4];
        float color[4];
    } triangleVertices[]{
        // Direct3D是以左手作为前面背面顶点排列的依据
        {.position { 0.0f, 0.75f, 0.0f, 1.0f }, .color { 0.9f, 0.1f, 0.1f, 1.0f } },    // top center
        {.position { 0.75f, -0.75f, 0.0f, 1.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },  // bottom right
        {.position { -0.75f, -0.75f, 0.0f, 1.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } }  // bottom left
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

    const D3D12_RESOURCE_DESC vbResourceDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = sizeof(triangleVertices),
        .Height = 1U,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc {.Count = 1U, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* constantBuffer = nullptr;

    auto const result = std::make_tuple(uploadDevHostBuffer, vertexBuffer, constantBuffer);

    // Create vertexBuffer on GPU side.
    HRESULT hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vbResourceDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&vertexBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for vertex buffer failed: %ld\n", hRes);
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

    // upload vertex data to vertex buffer
    void* hostMemPtr = nullptr;
    hRes = uploadDevHostBuffer->Map(0, nullptr, &hostMemPtr);
    if (FAILED(hRes))
    {
        fprintf(stderr, "Map vertex buffer failed: %ld\n", hRes);
        return result;
    }

    memcpy(hostMemPtr, triangleVertices, sizeof(triangleVertices));
    uploadDevHostBuffer->Unmap(0, nullptr);

    WriteToDeviceResourceAndSync(commandList, vertexBuffer, uploadDevHostBuffer, 0U, 0U, sizeof(triangleVertices));

    hRes = commandList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close command list failed: %ld\n", hRes);
        return result;
    }

    // Upload data to constant buffer
    hRes = constantBuffer->Map(0U, nullptr, &hostMemPtr);
    if (FAILED(hRes))
    {
        fprintf(stderr, "Map constant buffer failed: %ld\n", hRes);
        return result;
    }

    // Set translations
    struct { float rotAngle; float xOffset; float yOffset; float zOffset; } *pTranslations;
    pTranslations = (decltype(pTranslations))hostMemPtr;
    pTranslations->rotAngle = s_currAngle;
    pTranslations->xOffset = s_currX;
    pTranslations->yOffset = s_currY;
    pTranslations->zOffset = s_currZ;

    constantBuffer->Unmap(0, nullptr);

    // Execute the command list to complete the copy operation
    ID3D12CommandList* const ppCommandLists[] = { (ID3D12CommandList*)commandList };
    commandQueue->ExecuteCommandLists((UINT)std::size(ppCommandLists), ppCommandLists);

    // Initialize the vertex buffer view.
    const D3D12_VERTEX_BUFFER_VIEW vertexBufferView{
        .BufferLocation = vertexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = (uint32_t)sizeof(triangleVertices),
        .StrideInBytes = sizeof(triangleVertices[0])
    };

    // Record commands to the command list bundle.
    commandBundleList->SetGraphicsRootSignature(rootSignature);
    //commandBundleList->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
    // There's no need to invoke SetDescriptorHeaps when using Root Constant Buffer View.
    commandBundleList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());    // rootParameters[0]   
    commandBundleList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandBundleList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandBundleList->DrawInstanced(3U, 1U, 0U, 0U);

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

    return std::make_tuple(uploadDevHostBuffer, vertexBuffer, constantBuffer);
}

auto CreateProjectionTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue *commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                    std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>
{
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;
    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* constantBuffer = nullptr;
    bool success = false;

    auto const result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, uploadDevHostBuffer, vertexBuffer, constantBuffer, success);

    rootSignature = CreateRootSignature(d3d_device, true);
    if (rootSignature == nullptr) return result;

    do
    {
        auto const pipelineResult = CreatePipelineStateObject(d3d_device, commandAllocator, commandBundleAllocator, rootSignature);
        pipelineState = std::get<0>(pipelineResult);
        commandList = std::get<1>(pipelineResult);
        commandBundle = std::get<2>(pipelineResult);

        if (pipelineState == nullptr || commandList == nullptr || commandBundle == nullptr) break;

        auto const renderVertexBufferResult = CreateVertexBuffer(d3d_device, rootSignature, commandQueue, commandList, commandBundle);
        uploadDevHostBuffer = std::get<0>(renderVertexBufferResult);
        vertexBuffer = std::get<1>(renderVertexBufferResult);
        constantBuffer = std::get<2>(renderVertexBufferResult);

        if (uploadDevHostBuffer == nullptr || vertexBuffer == nullptr || constantBuffer == nullptr) break;

        success = true;
    }
    while (false);

    return std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, uploadDevHostBuffer, vertexBuffer, constantBuffer, success);
}

auto ProjectionTestTranslateProcess(const TranslationType& transType) -> void
{
    switch (transType)
    {
    case TranslationType::ROTATE_CLOCKWISE:
        s_currAngle -= 1.0f;
        break;

    case TranslationType::ROTATE_COUNTER_CLOCKWISE:
        s_currAngle += 1.0f;
        break;

    case TranslationType::MOVE_LEFT:
        s_currX -= 0.05f;
        break;

    case TranslationType::MOVE_RIGHT:
        s_currX += 0.05f;
        break;

    case TranslationType::MOVE_UP:
        s_currY += 0.05f;
        break;

    case TranslationType::MOVE_DOWN:
        s_currY -= 0.05f;
        break;

    case TranslationType::MOVE_NEAR:
        s_currZ += 0.05f;
        break;

    case TranslationType::MOVE_FAR:
        s_currZ -= 0.05f;
        break;
    }

    if (transType == TranslationType::ROTATE_CLOCKWISE || transType == TranslationType::ROTATE_COUNTER_CLOCKWISE)
    {
        if (s_currAngle >= 360.0f || s_currAngle <= -360.0f) {
            s_currAngle = 0.0f;
        }
    }
}

auto ProjectionTestFetchTranslationSet() -> CommonTranslationSet
{
    return CommonTranslationSet{
        .rotAngle = s_currAngle,
        .xOffset = s_currX,
        .yOffset = s_currY,
        .zOffset = s_currZ
    };
}

