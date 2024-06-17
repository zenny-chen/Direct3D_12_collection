#include "common.h"


static constexpr UINT TEXTURE_SIZE = WINDOW_WIDTH / 8U;
static constexpr UINT TEXTURE_SAMPLE_COUNT = 1U;
static constexpr bool MSAA_RENDER_TARGET_NEED_RESOLVE = true && TEXTURE_SAMPLE_COUNT > 1U;
static constexpr UINT FORCE_SAMPLE_COUNT = 4U;
static constexpr UINT GRAPHICS_PIPELINE_SAMPLE_MASK = UINT32_MAX * 1U;
static constexpr UINT uavBufferSize = 64U;

enum CBV_SRV_UAV_SLOT_ID
{
    SRV_DEPTH_TEXTURE_SLOT,
    UAV_PS_INVOKE_COUNT_SLOT,
    UAV_COMPUTE_OUTPUT_SLOT,
    CBV_SRV_UAV_SLOT_COUNT
};

static auto CreateRootSignature(ID3D12Device* d3d_device) -> ID3D12RootSignature*
{
    ID3D12RootSignature* rootSignature = nullptr;

    const D3D12_DESCRIPTOR_RANGE descRanges[] {
        // t0 (for presentation)
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        },
        // u0 (for render target texture rendering)
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        },
        // u1 (for compute output depth destination buffer)
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 1,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        }
    };

    const D3D12_ROOT_PARAMETER rootParameters[] {
        // s0 (for presentation)
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[0]
            },
            // This texture buffer will be accessed in a pixel shader and a compute shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        // u0 (for render target texture rendering)
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[1]
            },
            // This unordered access view buffer will just be accessed in a pixel shader
            .ShaderVisibility =  D3D12_SHADER_VISIBILITY_PIXEL
        },
        // u1 (for compute output depth destination buffer)
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[2]
            },
            // This unordered access view buffer will be accessed in a compute shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        // b0 (for setting z-offset in render target texture rendering)
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants {
                .ShaderRegister = 0,
                .RegisterSpace = 0,
                .Num32BitValues = 1
            },
            // This 32-bit constant will be accessed in a vertex shader and a compute shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
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

// @return [rtvDescriptorHeap, rtTexture, resolvedRTTexture]
static auto CreateRenderTargetViewForTexture(ID3D12Device* d3d_device) -> std::tuple<ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*>
{
    ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
    ID3D12Resource* rtTexture = nullptr;
    ID3D12Resource* resolvedRTTexture = nullptr;

    auto result = std::make_tuple(rtvDescriptorHeap, rtTexture, resolvedRTTexture);

    // Describe and create a render target view (RTV) descriptor heap.
    const D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0
    };
    HRESULT hRes = d3d_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateDescriptorHeap for render target view failed: %ld\n", hRes);
        return result;
    }

    const D3D12_HEAP_PROPERTIES defaultHeapProperties{
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1
    };

    const D3D12_RESOURCE_DESC rtResourceDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = TEXTURE_SIZE,
        .Height = TEXTURE_SIZE,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = RENDER_TARGET_BUFFER_FOMRAT,
        .SampleDesc {.Count = TEXTURE_SAMPLE_COUNT, .Quality = 0U },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
    };

    const D3D12_RESOURCE_DESC rtResolveResourceDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = TEXTURE_SIZE,
        .Height = TEXTURE_SIZE,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = RENDER_TARGET_BUFFER_FOMRAT,
        .SampleDesc {.Count = 1U, .Quality = 0U },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    const D3D12_CLEAR_VALUE rtOptClearValue{
        .Format = RENDER_TARGET_BUFFER_FOMRAT,
        .Color { 0.5f, 0.6f, 0.5f, 1.0f }
    };

    const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{
        .Format = RENDER_TARGET_BUFFER_FOMRAT,
        .ViewDimension = TEXTURE_SAMPLE_COUNT > 1U ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D,
        .Texture2D { .MipSlice = 0, .PlaneSlice = 0 }
    };

    // Create frame resources
    do
    {
        // Create the MSAA RTV texture
        hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &rtResourceDesc, D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                                                &rtOptClearValue, IID_PPV_ARGS(&rtTexture));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommittedResource for texture render target failed: %ld!\n", hRes);
            break;
        }

        // Create the resolved RT texture
        hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &rtResolveResourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                    nullptr, IID_PPV_ARGS(&resolvedRTTexture));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommittedResource for resolved texture render target failed: %ld!\n", hRes);
            break;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        d3d_device->CreateRenderTargetView(rtTexture, &rtvDesc, rtvHandle);

        const UINT rtvDescriptorSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        rtvHandle.ptr += rtvDescriptorSize;

        return std::make_tuple(rtvDescriptorHeap, rtTexture, resolvedRTTexture);
    }
    while (false);

    if (rtvDescriptorHeap != nullptr) {
        rtvDescriptorHeap->Release();
    }
    if (rtTexture != nullptr) {
        rtTexture->Release();
    }
    if (resolvedRTTexture != nullptr) {
        resolvedRTTexture->Release();
    }

    return result;
}

// @return [pipelineState, commandList, commandBundleList, cbv_uavDescriptorHeap]
static auto CreatePipelineStateObjectForRenderTexture(ID3D12Device* d3d_device, ID3D12CommandAllocator *commandAllocator, ID3D12CommandAllocator* commandBundleAllocator, ID3D12RootSignature* rootSignature) ->
                                        std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;
    ID3D12DescriptorHeap* cbv_uavDescriptorHeap = nullptr;

    auto result = std::make_tuple(pipelineState, commandList, commandBundleList, cbv_uavDescriptorHeap);

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("cso/tir.vert.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("cso/tir.frag.cso");

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
            .SampleMask = GRAPHICS_PIPELINE_SAMPLE_MASK,
            // Use the default rasterizer state
            .RasterizerState {
                .FillMode = D3D12_FILL_MODE_SOLID,
                .CullMode = D3D12_CULL_MODE_BACK,
                .FrontCounterClockwise = FALSE,
                .DepthBias = 0,
                .DepthBiasClamp = 0.0f,
                .SlopeScaledDepthBias = 0.0f,
                .DepthClipEnable = TRUE,
                .MultisampleEnable = TEXTURE_SAMPLE_COUNT > 1U,
                .AntialiasedLineEnable = FALSE,
                .ForcedSampleCount = FORCE_SAMPLE_COUNT,
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
            .DSVFormat = DXGI_FORMAT_UNKNOWN,   // ATTENTION!! When ForcedSampleCount is not 0, DSVFormat MUST BE DXGI_FORMAT_UNKNOWN to indicate that the DepthStencilView is not bound.
            .SampleDesc {
                .Count = TEXTURE_SAMPLE_COUNT,
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

        const D3D12_DESCRIPTOR_HEAP_DESC cbv_uavHeapDesc {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = CBV_SRV_UAV_SLOT_ID::CBV_SRV_UAV_SLOT_COUNT,  // This descriptor heap is for all of CBV, UAV and SRV buffers.
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0
        };
        hRes = d3d_device->CreateDescriptorHeap(&cbv_uavHeapDesc, IID_PPV_ARGS(&cbv_uavDescriptorHeap));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateDescriptorHeap for unordered access view failed: %ld\n", hRes);
            return result;
        }

        result = std::make_tuple(pipelineState, commandList, commandBundleList, cbv_uavDescriptorHeap);
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

static auto CreatePipelineStateObjectForPresentation(ID3D12Device* d3d_device, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator, ID3D12RootSignature* rootSignature) ->
                                                    std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;
    ID3D12DescriptorHeap* descriptorHeap = nullptr;

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("cso/cr_present.vert.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("cso/cr_present.frag.cso");

    do
    {
        if (vertexShaderObj.pShaderBytecode == nullptr || vertexShaderObj.BytecodeLength == 0) break;
        if (pixelShaderObj.pShaderBytecode == nullptr || pixelShaderObj.BytecodeLength == 0) break;

        // Define the vertex input layout used for Input Assembler
        const D3D12_INPUT_ELEMENT_DESC inputElementDescs[]{
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
                .MultisampleEnable = USE_MSAA_RENDER_TARGET != 0,
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
            .DSVFormat = DXGI_FORMAT_UNKNOWN,
            .SampleDesc {
                .Count = USE_MSAA_RENDER_TARGET == 0 ? 1U : USE_MSAA_RENDER_TARGET,
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
    }
    while (false);

    if (vertexShaderObj.pShaderBytecode != nullptr) {
        free((void*)vertexShaderObj.pShaderBytecode);
    }
    if (pixelShaderObj.pShaderBytecode != nullptr) {
        free((void*)pixelShaderObj.pShaderBytecode);
    }

    return std::make_tuple(pipelineState, commandList, commandBundleList);
}

// @return [uploadDevHostBuffer, vertexBuffer, uavBuffer, readbackDevHostBuffer, readBackTextureHostBuffer, uavCompOutBuffer]
static auto CreateVertexBufferForRenderTexture(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12CommandQueue *commandQueue, ID3D12GraphicsCommandList* commandList,
                                            ID3D12GraphicsCommandList* commandBundle, ID3D12DescriptorHeap* cbv_uavDescriptorHeap) ->
                                            std::tuple<ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*>
{
    struct Vertex
    {
        float position[4];
        float color[4];
    } triVertices[] {
        // top center
        { { 0.0f, 0.25f, 0.0f, 1.0f }, { 0.9f, 0.1f, 0.1f, 1.0f } },
        // bottom right
        { { 0.25f, -0.25f, 0.0f, 1.0f }, { 0.1f, 0.9f, 0.1f, 1.0f } },
        // bottom left
        { { -0.25f, -0.25f, 0.0f, 1.0f }, { 0.1f, 0.1f, 0.9f, 1.0f } }
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
        .Width = sizeof(triVertices),
        .Height = 1U,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc {.Count = 1U, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* readbackDevHostBuffer = nullptr;
    ID3D12Resource* readBackTextureHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* uavBuffer = nullptr;
    ID3D12Resource* uavCompOutBuffer = nullptr;

    auto result = std::make_tuple(uploadDevHostBuffer, vertexBuffer, uavBuffer, readbackDevHostBuffer, readBackTextureHostBuffer, uavCompOutBuffer);

    // Create vertexBuffer on GPU side.
    HRESULT hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vbResourceDesc,
                                                    D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&vertexBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for vertex buffer failed: %ld\n", hRes);
        return result;
    }

    // Create uploadDevHostBuffer with host visible for upload
    D3D12_RESOURCE_DESC uploadResourceDesc = vbResourceDesc;
    uploadResourceDesc.Width += uavBufferSize;
    hRes = d3d_device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadResourceDesc,
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

    const D3D12_RESOURCE_DESC uavResourceDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = uavBufferSize,
        .Height = 1U,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc {.Count = 1U, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
    };
    hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavResourceDesc,
                                            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&uavBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for unordered access view buffer failed: %ld\n", hRes);
        return result;
    }

    D3D12_RESOURCE_DESC uavCompOutResourceDesc = uavResourceDesc;
    uavCompOutResourceDesc.Width = TEXTURE_SIZE * TEXTURE_SIZE * UINT(sizeof(float));
    hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavCompOutResourceDesc,
                                            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&uavCompOutBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for uavCompOutBuffer failed: %ld\n", hRes);
        return result;
    }

    D3D12_HEAP_PROPERTIES readbackHeapProperties = uploadHeapProperties;
    readbackHeapProperties.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC readbackResourceDesc = uavResourceDesc;
    readbackResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hRes = d3d_device->CreateCommittedResource(&readbackHeapProperties, D3D12_HEAP_FLAG_NONE, &readbackResourceDesc,
                                            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackDevHostBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for readbackDevHostBuffer failed: %ld\n", hRes);
        return result;
    }

    const D3D12_RESOURCE_DESC readbackTextureResourceDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = uavCompOutResourceDesc.Width,
        .Height = 1U,
        .DepthOrArraySize = 1U,
        .MipLevels = 1U,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc {.Count = 1, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };
    hRes = d3d_device->CreateCommittedResource(&readbackHeapProperties, D3D12_HEAP_FLAG_NONE, &readbackTextureResourceDesc,
                                            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readBackTextureHostBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for readBackTextureHostBuffer failed: %ld\n", hRes);
        return result;
    }

    const UINT cbv_uavDescriptorSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE srvCPUDescHandle = cbv_uavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE uavCPUDescHandle = srvCPUDescHandle;
    uavCPUDescHandle.ptr += UAV_PS_INVOKE_COUNT_SLOT * cbv_uavDescriptorSize;

    // Create the unordered access buffer view
    const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer {
            .FirstElement = 0,
            .NumElements = uavBufferSize / UINT(sizeof(unsigned)),
            .StructureByteStride = UINT(sizeof(unsigned)),
            .CounterOffsetInBytes = 0,
            .Flags = D3D12_BUFFER_UAV_FLAG_NONE
        }
    };
    d3d_device->CreateUnorderedAccessView(uavBuffer, nullptr, &uavDesc, uavCPUDescHandle);

    void* hostMemPtr = nullptr;
    hRes = uploadDevHostBuffer->Map(0, nullptr, &hostMemPtr);
    if (FAILED(hRes))
    {
        fprintf(stderr, "Map vertex buffer failed: %ld\n", hRes);
        return result;
    }

    // Copy vertex data
    memcpy(hostMemPtr, triVertices, sizeof(triVertices));

    // Clear the UAV data
    memset((void*)(uintptr_t(hostMemPtr) + sizeof(triVertices)), 0, uavBufferSize);

    uploadDevHostBuffer->Unmap(0, nullptr);

    WriteToDeviceResourceAndSync(commandList, vertexBuffer, uploadDevHostBuffer, 0U, 0U, sizeof(triVertices));
    WriteToDeviceResourceAndSync(commandList, uavBuffer, uploadDevHostBuffer, 0U, sizeof(triVertices), uavBufferSize);

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
    const D3D12_VERTEX_BUFFER_VIEW vertexBufferView{
        .BufferLocation = vertexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = UINT(sizeof(triVertices)),
        .StrideInBytes = sizeof(triVertices[0])
    };

    D3D12_GPU_DESCRIPTOR_HANDLE srvGPUDescHandle = cbv_uavDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE uavGPUDescHandle = srvGPUDescHandle;
    uavGPUDescHandle.ptr += UAV_PS_INVOKE_COUNT_SLOT * cbv_uavDescriptorSize;

    ID3D12DescriptorHeap* const descHeaps[]{ cbv_uavDescriptorHeap };

    // Record commands to the command list bundle.
    commandBundle->SetGraphicsRootSignature(rootSignature);
    // ATTENTION: SetDescriptorHeaps should be set into command bundle list as well as command list
    commandBundle->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
    commandBundle->SetGraphicsRootDescriptorTable(1U, uavGPUDescHandle);    // rootParameters[1]
    constexpr union { float f; UINT i; } constValue{ .f = -2.166f };
    commandBundle->SetGraphicsRoot32BitConstant(3U, constValue.i, 0);       // rootParameters[3]
    commandBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandBundle->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandBundle->DrawInstanced((UINT)std::size(triVertices), 1U, 0U, 0U);

    // End of the record
    hRes = commandBundle->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close command bundle failed: %ld\n", hRes);
        return result;
    }

    // Wait for the command list to execute;
    // we are reusing the same command list in our main loop but for now,
    // we just want to wait for setup to complete before continuing.
    WaitForPreviousFrame(commandQueue);

    return std::make_tuple(uploadDevHostBuffer, vertexBuffer, uavBuffer, readbackDevHostBuffer, readBackTextureHostBuffer, uavCompOutBuffer);
}

// @return std::make_pair(uploadDevHostBuffer, vertexBuffer)
static auto CreateVertexBufferForPresentation(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12CommandQueue* commandQueue,
                                            ID3D12GraphicsCommandList* commandList, ID3D12GraphicsCommandList* commandBundleList,
                                            ID3D12DescriptorHeap* descriptorHeap, ID3D12Resource* rtTexture) -> std::pair<ID3D12Resource*, ID3D12Resource*>
{
    const struct Vertex
    {
        float position[4];
    } squareVertices[]{
        // Direct3D是以左手作为前面背面顶点排列的依据
        { .position { -1.0f, 1.0f, 0.0f, 1.0f } },     // top left
        { .position { 1.0f, 1.0f, 0.0f, 1.0f } },      // top right
        { .position { -1.0f, -1.0f, 0.0f, 1.0f } },    // bottom left
        { .position { 1.0f, -1.0f, 0.0f, 1.0f } }      // bottom right
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
        .Width = sizeof(squareVertices),
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

    auto result = std::make_pair(uploadDevHostBuffer, vertexBuffer);

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

    void* hostMemPtr = nullptr;
    hRes = uploadDevHostBuffer->Map(0, nullptr, &hostMemPtr);
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
    const D3D12_VERTEX_BUFFER_VIEW vertexBufferView{
        .BufferLocation = vertexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = (uint32_t)sizeof(squareVertices),
        .StrideInBytes = sizeof(squareVertices[0])
    };

    const UINT descriptorIncrSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Fetch CBV and UAV CPU descriptor handles
    D3D12_CPU_DESCRIPTOR_HANDLE textureSRVCPUDescHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    textureSRVCPUDescHandle.ptr += SRV_DEPTH_TEXTURE_SLOT * descriptorIncrSize;

    // Create the texture shader resource view
    const D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc{
        .Format = RENDER_TARGET_BUFFER_FOMRAT,
        .ViewDimension = TEXTURE_SAMPLE_COUNT > 1U ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D {
            .MostDetailedMip = 0,
            .MipLevels = 1,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0f
        }
    };
    d3d_device->CreateShaderResourceView(rtTexture, &textureSRVDesc, textureSRVCPUDescHandle);

    // Fetch CBV and UAV GPU descriptor handles
    D3D12_GPU_DESCRIPTOR_HANDLE textureSRVGPUDescHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    textureSRVCPUDescHandle.ptr += descriptorIncrSize;

    // Record commands to the command list bundle.
    commandBundleList->SetGraphicsRootSignature(rootSignature);
    ID3D12DescriptorHeap* const descHeaps[]{ descriptorHeap };
    // ATTENTION: SetDescriptorHeaps should be set into command bundle list as well as command list
    commandBundleList->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
    commandBundleList->SetGraphicsRootDescriptorTable(0, textureSRVGPUDescHandle);  // rootParameters[0]
    commandBundleList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    commandBundleList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandBundleList->DrawInstanced((UINT)std::size(squareVertices), 1U, 0U, 0U);

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

    return std::make_pair(uploadDevHostBuffer, vertexBuffer);
}

static auto PopulateCommandList(ID3D12GraphicsCommandList* commandBundle, ID3D12GraphicsCommandList* commandList,
                                ID3D12DescriptorHeap* rtvDescriptorHeap, ID3D12DescriptorHeap* cbv_uavDescriptorHeap, ID3D12QueryHeap* queryHeap,
                                ID3D12Resource* renderTarget, ID3D12Resource* resolvedRTTexture, ID3D12Resource* uavBuffer, ID3D12Resource* readbackDevHostBuffer) -> bool
{
    // Record commands to the command list
    // Set necessary state.
    constexpr auto viewportWidth = LONG(TEXTURE_SIZE);
    constexpr auto viewportHeight = LONG(TEXTURE_SIZE);

    const D3D12_VIEWPORT viewPort{
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = FLOAT(viewportWidth),
        .Height = FLOAT(viewportHeight),
        .MinDepth = 0.0f,
        .MaxDepth = 3.0f
    };
    commandList->RSSetViewports(1, &viewPort);

    const D3D12_RECT scissorRect{
        .left = 0,
        .top = 0,
        .right = viewportWidth,
        .bottom = viewportHeight
    };
    commandList->RSSetScissorRects(1, &scissorRect);

    const D3D12_RESOURCE_BARRIER renderBarriers[]{
        // render target state transition
        {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition {
                .pResource = renderTarget,
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
            }
        }
    };
    commandList->ResourceBarrier((UINT)std::size(renderBarriers), renderBarriers);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float clearColor[] = { 0.5f, 0.6f, 0.5f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    ID3D12DescriptorHeap* const descHeaps[]{ cbv_uavDescriptorHeap };
    commandList->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);

    // Insert the begin query
    commandList->BeginQuery(queryHeap, D3D12_QUERY_TYPE_OCCLUSION, 0U);

    // Execute the bundle to the command list
    commandList->ExecuteBundle(commandBundle);

    // Insert the end query
    commandList->EndQuery(queryHeap, D3D12_QUERY_TYPE_OCCLUSION, 0U);

    // Make the render target as shader resource view, or resolve the render target to the destniation resolved texture
    if (MSAA_RENDER_TARGET_NEED_RESOLVE)
    {
        const D3D12_RESOURCE_BARRIER storeBarriers[]{
            // render target state transition
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition {
                    .pResource = renderTarget,
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
                    .StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE
                }
            },
            // resovled render target texture transition
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition {
                    .pResource = resolvedRTTexture,
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = D3D12_RESOURCE_STATE_COMMON,
                    .StateAfter = D3D12_RESOURCE_STATE_RESOLVE_DEST
                }
            }
        };
        commandList->ResourceBarrier((UINT)std::size(storeBarriers), storeBarriers);

        // Resolve MSAA render target and depth stencil texture to resovled textures
        commandList->ResolveSubresource(resolvedRTTexture, 0, renderTarget, 0, RENDER_TARGET_BUFFER_FOMRAT);

        const D3D12_RESOURCE_BARRIER resolvedBarriers[]{
            // resovled render target texture transition
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition {
                    .pResource = resolvedRTTexture,
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST,
                    .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                }
            }
        };
        commandList->ResourceBarrier((UINT)std::size(resolvedBarriers), resolvedBarriers);
    }
    else
    {
        const D3D12_RESOURCE_BARRIER storeBarriers[]{
            // render target state transition
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition {
                    .pResource = renderTarget,
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
                    .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                }
            }
        };
        commandList->ResourceBarrier((UINT)std::size(storeBarriers), storeBarriers);
    }

    SyncAndReadFromDeviceResource(commandList, uavBufferSize, readbackDevHostBuffer, uavBuffer);

    // Resolve the Query Data
    commandList->ResolveQueryData(queryHeap, D3D12_QUERY_TYPE_OCCLUSION, 0U, 1U, readbackDevHostBuffer, 8U * 4U);

    // End of the record
    HRESULT hRes = commandList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close command list in populate commands failed: %ld\n", hRes);
        return false;
    }

    return true;
}

auto CreateTargetIndependentTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue *commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                    std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>
{
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12PipelineState* computePipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;
    ID3D12GraphicsCommandList* computeCommandList = nullptr;
    ID3D12GraphicsCommandList* computeCommandBundle = nullptr;
    ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
    ID3D12DescriptorHeap* cbv_uavDescriptorHeap = nullptr;
    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* rtTexture = nullptr;
    ID3D12Resource* resolvedRTTexture = nullptr;
    ID3D12Resource* uavBuffer = nullptr;
    ID3D12Resource* uavCompOutBuffer = nullptr;
    ID3D12Resource* readbackDevHostBuffer = nullptr;
    ID3D12Resource* readBackTextureHostBuffer = nullptr;
    bool success = false;

    auto result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, cbv_uavDescriptorHeap, rtvDescriptorHeap, uploadDevHostBuffer, vertexBuffer, rtTexture, success);

    rootSignature = CreateRootSignature(d3d_device);
    if (rootSignature == nullptr) return result;

    ID3D12QueryHeap* queryHeap = nullptr;
    // Create the query heap for occlusion query
    const D3D12_QUERY_HEAP_DESC queryHeapDesc{
        .Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION,
        .Count = 1U,
        .NodeMask = 0U
    };
    const HRESULT hRes = d3d_device->CreateQueryHeap(&queryHeapDesc, IID_ID3D12QueryHeap, (void**)&queryHeap);
    if (FAILED(hRes))
    {
        fprintf(stderr, "Failed CreateQueryHeap: %ld\n", hRes);
        return result;
    }

    auto const rtTexRes = CreateRenderTargetViewForTexture(d3d_device);
    rtvDescriptorHeap = std::get<0>(rtTexRes);
    rtTexture = std::get<1>(rtTexRes);
    resolvedRTTexture = std::get<2>(rtTexRes);

    auto const pipelineResult = CreatePipelineStateObjectForRenderTexture(d3d_device, commandAllocator, commandBundleAllocator, rootSignature);
    pipelineState = std::get<0>(pipelineResult);
    commandList = std::get<1>(pipelineResult);
    commandBundle = std::get<2>(pipelineResult);
    cbv_uavDescriptorHeap = std::get<3>(pipelineResult);

    auto const renderVertexBufferResult = CreateVertexBufferForRenderTexture(d3d_device, rootSignature, commandQueue, commandList, commandBundle, cbv_uavDescriptorHeap);
    uploadDevHostBuffer = std::get<0>(renderVertexBufferResult);
    vertexBuffer = std::get<1>(renderVertexBufferResult);
    uavBuffer = std::get<2>(renderVertexBufferResult);
    readbackDevHostBuffer = std::get<3>(renderVertexBufferResult);
    readBackTextureHostBuffer = std::get<4>(renderVertexBufferResult);
    uavCompOutBuffer = std::get<5>(renderVertexBufferResult);

    do
    {
        if (!ResetCommandAllocatorAndList(commandAllocator, commandList, pipelineState)) break;

        if (!PopulateCommandList(commandBundle, commandList, rtvDescriptorHeap, cbv_uavDescriptorHeap, queryHeap,
                                rtTexture, resolvedRTTexture, uavBuffer, readbackDevHostBuffer)) break;

        // Execute the command list.
        ID3D12CommandList* const ppCommandLists[] = { (ID3D12CommandList*)commandList };
        commandQueue->ExecuteCommandLists((UINT)std::size(ppCommandLists), ppCommandLists);

        if (!WaitForPreviousFrame(commandQueue)) break;

        // Read back the pixel shader invocation count
        unsigned* hostMemPtr = nullptr;
        HRESULT hRes = readbackDevHostBuffer->Map(0, nullptr, (void**)&hostMemPtr);
        if (FAILED(hRes))
        {
            fprintf(stderr, "Map read back buffer failed: %ld\n", hRes);
            break;
        }

        unsigned long long* queryPtr = (unsigned long long*)hostMemPtr;

        printf("Current pixel shader invocation count: %u\n", hostMemPtr[0]);
        printf("Occlusion Query value: %llu\n", queryPtr[4]);

        readbackDevHostBuffer->Unmap(0, nullptr);

        success = true;
    }
    while (false);

    result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, cbv_uavDescriptorHeap, rtvDescriptorHeap, uploadDevHostBuffer, vertexBuffer, rtTexture, success);

    if (!success) return result;

    pipelineState->Release();
    pipelineState = nullptr;

    queryHeap->Release();

    uavBuffer->Release();
    uavCompOutBuffer->Release();
    readbackDevHostBuffer->Release();
    readBackTextureHostBuffer->Release();

    if (computePipelineState != nullptr) {
        computePipelineState->Release();
    }
    if (computeCommandList != nullptr) {
        computeCommandList->Release();
    }
    if (computeCommandBundle != nullptr) {
        computeCommandBundle->Release();
    }

    if (MSAA_RENDER_TARGET_NEED_RESOLVE)
    {
        rtTexture->Release();
        rtTexture = resolvedRTTexture;
    }
    else
    {
        if (resolvedRTTexture != nullptr) {
            resolvedRTTexture->Release();
        }
    }
    
    commandList->Release();
    commandList = nullptr;

    commandBundle->Release();
    commandBundle = nullptr;

    uploadDevHostBuffer->Release();
    uploadDevHostBuffer = nullptr;

    vertexBuffer->Release();
    vertexBuffer = nullptr;
    
    result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, cbv_uavDescriptorHeap, rtvDescriptorHeap, uploadDevHostBuffer, vertexBuffer, rtTexture, success);

    success = true;

    auto const pipelinePresentResult = CreatePipelineStateObjectForPresentation(d3d_device, commandAllocator, commandBundleAllocator, rootSignature);
    pipelineState = std::get<0>(pipelinePresentResult);
    commandList = std::get<1>(pipelinePresentResult);
    commandBundle = std::get<2>(pipelinePresentResult);

    if (pipelineState == nullptr || commandList == nullptr || commandBundle == nullptr) {
        success = false;
    }

    auto const presentVertexBufferResult = CreateVertexBufferForPresentation(d3d_device, rootSignature, commandQueue, commandList, commandBundle, cbv_uavDescriptorHeap, rtTexture);
    uploadDevHostBuffer = presentVertexBufferResult.first;
    vertexBuffer = presentVertexBufferResult.second;

    return std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, cbv_uavDescriptorHeap, rtvDescriptorHeap,
                        uploadDevHostBuffer, vertexBuffer, rtTexture, success);
}

