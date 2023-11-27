#include "common.h"


#define TEST_UNCERTAINTY_REGION     0

static constexpr D3D12_FILL_MODE USE_FILL_MODE = D3D12_FILL_MODE_SOLID;
static constexpr D3D12_CONSERVATIVE_RASTERIZATION_MODE USE_CONSERVATIVE_RASTERIZATION_MODE = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
static constexpr D3D_PRIMITIVE_TOPOLOGY RENDER_TEXTURE_USE_PRIMITIVE_TOPOLOGY = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

// DO NOT configure this constant
static constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE RENDER_TEXTURE_USE_PRIMITIVE_TOPOLOGY_TYPE = []() constexpr -> D3D12_PRIMITIVE_TOPOLOGY_TYPE {
    switch (RENDER_TEXTURE_USE_PRIMITIVE_TOPOLOGY)
    {
    case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
    case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    default:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}();

static constexpr UINT TEXTURE_SIZE = WINDOW_WIDTH / 8;
static constexpr UINT TEXTURE_SAMPLE_COUNT = 1U;

static auto CreateRootSignature(ID3D12Device* d3d_device, bool isForRenderTexture) -> ID3D12RootSignature*
{
    ID3D12RootSignature* rootSignature = nullptr;

    const D3D12_DESCRIPTOR_RANGE descRange{
        // t0
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0,
        .RegisterSpace = 0,
        .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
    };

    const D3D12_ROOT_PARAMETER rootParameter{
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable {
            .NumDescriptorRanges = 1,
            .pDescriptorRanges = &descRange
        },
        // This texture buffer will just be accessed in a pixel shader
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
    };

    // Create a root signature.
    const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {
        .NumParameters = isForRenderTexture ? 0U : 1U,
        .pParameters = isForRenderTexture ? NULL : &rootParameter,
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

// @return [rtvDescriptorHeap, rtTexture]
static auto CreateRenderTargetViewForTexture(ID3D12Device* d3d_device) -> std::pair<ID3D12DescriptorHeap*, ID3D12Resource*>
{
    ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
    ID3D12Resource* rtTexture = nullptr;

    auto result = std::make_pair(rtvDescriptorHeap, rtTexture);

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

    const UINT rtvDescriptorSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

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

    const D3D12_CLEAR_VALUE optClearValue{
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
        // Create the RTV texture
        hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &rtResourceDesc, D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                                                &optClearValue, IID_PPV_ARGS(&rtTexture));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommittedResource for texture render target failed: %ld!\n", hRes);
            break;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        d3d_device->CreateRenderTargetView(rtTexture, &rtvDesc, rtvHandle);

        rtvHandle.ptr += rtvDescriptorSize;

        return std::make_pair(rtvDescriptorHeap, rtTexture);
    }
    while (false);

    if (rtvDescriptorHeap != nullptr) {
        rtvDescriptorHeap->Release();
    }
    if (rtTexture != nullptr) {
        rtTexture->Release();
    }

    return result;
}

static auto CreatePipelineStateObjectForRenderTexture(ID3D12Device* d3d_device, ID3D12CommandAllocator *commandAllocator, ID3D12CommandAllocator* commandBundleAllocator, ID3D12RootSignature* rootSignature) ->
                                        std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;

    auto result = std::make_tuple(pipelineState, commandList, commandBundleList);

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("shaders/cr.vert.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("shaders/cr.frag.cso");

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
                .FillMode = USE_FILL_MODE,
                .CullMode = D3D12_CULL_MODE_NONE,
                .FrontCounterClockwise = FALSE,
                .DepthBias = 0,
                .DepthBiasClamp = 0.0f,
                .SlopeScaledDepthBias = 0.0f,
                .DepthClipEnable = TRUE,
                .MultisampleEnable = TEXTURE_SAMPLE_COUNT > 1U,
                .AntialiasedLineEnable = FALSE,
                .ForcedSampleCount = 0,
                .ConservativeRaster = USE_CONSERVATIVE_RASTERIZATION_MODE
            },
            .DepthStencilState {
                .DepthEnable = TRUE,                            // FALSE
                .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
                .DepthFunc = D3D12_COMPARISON_FUNC_LESS,        // D3D12_COMPARISON_FUNC_NEVER
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
            .PrimitiveTopologyType = RENDER_TEXTURE_USE_PRIMITIVE_TOPOLOGY_TYPE,
            .NumRenderTargets = 1,
            .RTVFormats {
                // RTVFormats[0]
                { RENDER_TARGET_BUFFER_FOMRAT }
            },
            .DSVFormat = DXGI_FORMAT_UNKNOWN,
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

#if TEST_UNCERTAINTY_REGION

static auto CreatePipelineStateObjectForLinesRenderTexture(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature) -> ID3D12PipelineState*
{
    ID3D12PipelineState* pipelineState = nullptr;

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("shaders/cr.vert.cso");
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
                .MultisampleEnable = TEXTURE_SAMPLE_COUNT > 1U,
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
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
            .NumRenderTargets = 1,
            .RTVFormats {
                // RTVFormats[0]
                { RENDER_TARGET_BUFFER_FOMRAT }
            },
            .DSVFormat = DXGI_FORMAT_UNKNOWN,
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

    }
    while (false);

    if (vertexShaderObj.pShaderBytecode != nullptr) {
        free((void*)vertexShaderObj.pShaderBytecode);
    }
    if (pixelShaderObj.pShaderBytecode != nullptr) {
        free((void*)pixelShaderObj.pShaderBytecode);
    }

    return pipelineState;
}

#endif

static auto CreatePipelineStateObjectForPresentation(ID3D12Device* d3d_device, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator, ID3D12RootSignature* rootSignature) ->
                                                    std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;
    ID3D12DescriptorHeap* descriptorHeap = nullptr;

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("shaders/cr_present.vert.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("shaders/cr_present.frag.cso");

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

        const D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 1U,   // 1 descriptor for texture
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0
        };
        hRes = d3d_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&descriptorHeap));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateDescriptorHeap for texture shader resource view failed: %ld\n", hRes);
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

    return std::make_tuple(pipelineState, commandList, commandBundleList, descriptorHeap);
}

// @return std::make_tuple(uploadDevHostBuffer, vertexBuffer)
static auto CreateVertexBufferForRenderTexture(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12CommandQueue *commandQueue,
                                ID3D12GraphicsCommandList* commandList, ID3D12GraphicsCommandList* commandBundleList, ID3D12PipelineState* linePipelineState) -> std::pair<ID3D12Resource*, ID3D12Resource*>
{
    const struct Vertex
    {
        float position[4];
        float color[4];
    } triangleVertices[]{
        // Direct3D是以左手作为前面背面顶点排列的依据
#if TEST_UNCERTAINTY_REGION
        {.position { 0.0f / 32.0f + 1.0f / (32.0f * 256.0f), 4.0f / 32.0f, 0.0f, 1.0f}, .color {0.9f, 0.1f, 0.1f, 1.0f}},   // top
        {.position { 0.5f, 0.0f, 0.0f, 1.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },                                         // right
        {.position { 0.0f, -3.0f / 32.0f, 0.0f, 1.0f}, .color {0.1f, 0.1f, 0.9f, 1.0f}},                                    // bottom
        //{.position { 1.0f / (32.0f * 256.0f), 1.0f / 32.0f, 0.0f, 1.0f}, .color {0.9f, 0.1f, 0.1f, 1.0f}},                    // top
        //{.position { 1.0f / 32.0f, -1.0f / 32.0f, 0.0f, 1.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },                          // right
        //{.position { -2.0f / 32.0f + 1.0f / (32.0f * 256.0f), -1.0f / 32.0f, 0.0f, 1.0f}, .color {0.1f, 0.1f, 0.9f, 1.0f}},   // bottom

        // left lines
        {.position { -0.75f, 1.0f / 32.0f, 0.0f, 1.0f}, .color {0.1f, 0.1f, 0.9f, 1.0f}},           // top line left point
        {.position { -1.0f / 16.0f, 1.0f / 32.0f, 0.0f, 1.0f}, .color {0.1f, 0.1f, 0.9f, 1.0f}},    // top line right point

        {.position { -0.75f, 0.0f / 32.0f, 0.0f, 1.0f}, .color {0.9f, 0.1f, 0.1f, 1.0f}},           // center line left point
        {.position { -1.0f / 16.0f, 0.0f / 32.0f, 0.0f, 1.0f}, .color {0.9f, 0.1f, 0.1f, 1.0f}},    // center line right point

        {.position { -0.75f, -1.0f / 32.0f, 0.0f, 1.0f}, .color {0.1f, 0.9f, 0.1f, 1.0f}},          // bottom line left point
        {.position { -1.0f / 16.0f, -1.0f / 32.0f, 0.0f, 1.0f}, .color {0.1f, 0.9f, 0.1f, 1.0f}},   // bottom line right point
        // right lines
        {.position { 0.5f + 1.0f / 16.0f, 1.0f / 32.0f, 0.0f, 1.0f}, .color {0.1f, 0.1f, 0.9f, 1.0f}},  // top line left point
        {.position { 0.75f, 1.0f / 32.0f, 0.0f, 1.0f}, .color {0.1f, 0.1f, 0.9f, 1.0f}},                // top line right point

        {.position { 0.5f + 1.0f / 16.0f, 0.0f / 32.0f, 0.0f, 1.0f}, .color {0.9f, 0.1f, 0.1f, 1.0f}},  // center line left point
        {.position { 0.75f, 0.0f / 32.0f, 0.0f, 1.0f}, .color {0.9f, 0.1f, 0.1f, 1.0f}},                // center line right point

        {.position { 0.5f + 1.0f / 16.0f, -1.0f / 32.0f, 0.0f, 1.0f}, .color {0.1f, 0.9f, 0.1f, 1.0f}}, // bottom line left point
        {.position { 0.75f, -1.0f / 32.0f, 0.0f, 1.0f}, .color {0.1f, 0.9f, 0.1f, 1.0f}},               // bottom line right point
#else
        {.position { 0.0f, 0.75f, 0.0f, 1.0f }, .color { 0.9f, 0.1f, 0.1f, 1.0f } },    // top center
        {.position { 0.75f, -0.75f, 0.0f, 1.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },  // bottom right
        {.position { -0.75f, -0.75f, 0.0f, 1.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } }  // bottom left
#endif
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
    ID3D12Resource* offsetConstantBuffer = nullptr;
    ID3D12Resource* rotateConstantBuffer = nullptr;

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
    const D3D12_RANGE readRange{ 0, 0 };    // We do not intend to read from this resource on the CPU.
    hRes = uploadDevHostBuffer->Map(0, &readRange, &hostMemPtr);
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

    // Execute the command list to complete the copy operation
    ID3D12CommandList* const ppCommandLists[] = { (ID3D12CommandList*)commandList };
    commandQueue->ExecuteCommandLists((UINT)std::size(ppCommandLists), ppCommandLists);

    // Initialize the vertex buffer view.
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{
        .BufferLocation = vertexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = (uint32_t)sizeof(triangleVertices),
        .StrideInBytes = sizeof(triangleVertices[0])
    };

    // Record commands to the command list bundle.
    commandBundleList->SetGraphicsRootSignature(rootSignature);
    commandBundleList->IASetPrimitiveTopology(RENDER_TEXTURE_USE_PRIMITIVE_TOPOLOGY);
    commandBundleList->IASetVertexBuffers(0, 1, &vertexBufferView);

    const D3D12_SHADING_RATE_COMBINER combiners[D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT] = { D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_PASSTHROUGH };
    ((ID3D12GraphicsCommandList5*)commandBundleList)->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners);

    commandBundleList->DrawInstanced(3U, 1U, 0U, 0U);

    if (linePipelineState != nullptr)
    {
        commandBundleList->SetPipelineState(linePipelineState);

        commandBundleList->SetGraphicsRootSignature(rootSignature);
        commandBundleList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        commandBundleList->IASetVertexBuffers(0, 1, &vertexBufferView);
        commandBundleList->DrawInstanced(12U, 1U, 3U, 0U);
    }

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

// @return std::make_pair(uploadDevHostBuffer, vertexBuffer)
static auto CreateVertexBufferForPresentation(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12CommandQueue* commandQueue,
                                            ID3D12GraphicsCommandList* commandList, ID3D12GraphicsCommandList* commandBundleList,
                                            ID3D12DescriptorHeap* srvDescriptorHeap, ID3D12Resource* rtTexture) -> std::pair<ID3D12Resource*, ID3D12Resource*>
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

    // Fetch CBV and UAV CPU descriptor handles
    D3D12_CPU_DESCRIPTOR_HANDLE textureSRVCPUDescHandle = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    // Create the texture shader resource view
    const D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc{
        .Format = RENDER_TARGET_BUFFER_FOMRAT,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
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
    D3D12_GPU_DESCRIPTOR_HANDLE textureSRVGPUDescHandle = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

    // Record commands to the command list bundle.
    commandBundleList->SetGraphicsRootSignature(rootSignature);
    ID3D12DescriptorHeap* const descHeaps[]{ srvDescriptorHeap };
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

static auto PopulateCommandList(ID3D12GraphicsCommandList* commandBundle, ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* rtvDescriptorHeap, ID3D12Resource* renderTarget) -> bool
{
    // Record commands to the command list
    // Set necessary state.
    const D3D12_VIEWPORT viewPort{
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = FLOAT(TEXTURE_SIZE),
        .Height = FLOAT(TEXTURE_SIZE),
        .MinDepth = 0.0f,
        .MaxDepth = 3.0f
    };
    commandList->RSSetViewports(1, &viewPort);

    const D3D12_RECT scissorRect{
        .left = 0,
        .top = 0,
        .right = TEXTURE_SIZE,
        .bottom = TEXTURE_SIZE
    };
    commandList->RSSetScissorRects(1, &scissorRect);

    // Indicate that the back buffer will be used as a render target.
    const D3D12_RESOURCE_BARRIER renderBarrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition {
            .pResource = renderTarget,
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
            .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
        }
    };
    commandList->ResourceBarrier(1, &renderBarrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float clearColor[] = { 0.5f, 0.6f, 0.5f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Execute the bundle to the command list
    commandList->ExecuteBundle(commandBundle);

    // Indicate that the back buffer will now be used to present.
    const D3D12_RESOURCE_BARRIER storeBarrier{
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition {
            .pResource = renderTarget,
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
            .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        }
    };
    commandList->ResourceBarrier(1, &storeBarrier);

    // End of the record
    HRESULT hRes = commandList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close command list in populate commands failed: %ld\n", hRes);
        return false;
    }

    return true;
}

auto CreateConservativeRasterizationTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue *commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                    std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>
{
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;
    ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* rtTexture = nullptr;
    ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;
    bool success = false;

    auto result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, rtvDescriptorHeap, srvDescriptorHeap, uploadDevHostBuffer, vertexBuffer, rtTexture, success);

    rootSignature = CreateRootSignature(d3d_device, true);
    if (rootSignature == nullptr) return result;

    auto const rtTexRes = CreateRenderTargetViewForTexture(d3d_device);
    rtvDescriptorHeap = rtTexRes.first;
    rtTexture = rtTexRes.second;

    auto const pipelineResult = CreatePipelineStateObjectForRenderTexture(d3d_device, commandAllocator, commandBundleAllocator, rootSignature);
    pipelineState = std::get<0>(pipelineResult);
    commandList = std::get<1>(pipelineResult);
    commandBundle = std::get<2>(pipelineResult);

    ID3D12PipelineState* linePipelineState = nullptr;

#if TEST_UNCERTAINTY_REGION
    linePipelineState = CreatePipelineStateObjectForLinesRenderTexture(d3d_device, rootSignature);
#endif

    auto vertexBufferResult = CreateVertexBufferForRenderTexture(d3d_device, rootSignature, commandQueue, commandList, commandBundle, linePipelineState);
    uploadDevHostBuffer = std::get<0>(vertexBufferResult);
    vertexBuffer = std::get<1>(vertexBufferResult);

    do
    {
        if (!ResetCommandAllocatorAndList(commandAllocator, commandList, pipelineState)) break;

        if (!PopulateCommandList(commandBundle, commandList, rtvDescriptorHeap, rtTexture)) break;

        // Execute the command list.
        ID3D12CommandList* const ppCommandLists[] = { (ID3D12CommandList*)commandList };
        commandQueue->ExecuteCommandLists((UINT)std::size(ppCommandLists), ppCommandLists);

        if (!WaitForPreviousFrame(commandQueue)) break;

        success = true;
    }
    while (false);

    result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, rtvDescriptorHeap, srvDescriptorHeap, uploadDevHostBuffer, vertexBuffer, rtTexture, success);

    if (!success) return result;

    rootSignature->Release();
    rootSignature = nullptr;

    pipelineState->Release();
    pipelineState = nullptr;

    if (linePipelineState != nullptr) {
        linePipelineState->Release();
    }
    
    commandList->Release();
    commandList = nullptr;

    commandBundle->Release();
    commandBundle = nullptr;

    uploadDevHostBuffer->Release();
    uploadDevHostBuffer = nullptr;

    vertexBuffer->Release();
    vertexBuffer = nullptr;
    
    result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, rtvDescriptorHeap, srvDescriptorHeap, uploadDevHostBuffer, vertexBuffer, rtTexture, success);

    rootSignature = CreateRootSignature(d3d_device, false);
    if (rootSignature == nullptr) return result;

    success = true;

    auto const pipelinePresentResult = CreatePipelineStateObjectForPresentation(d3d_device, commandAllocator, commandBundleAllocator, rootSignature);
    pipelineState = std::get<0>(pipelinePresentResult);
    commandList = std::get<1>(pipelinePresentResult);
    commandBundle = std::get<2>(pipelinePresentResult);
    srvDescriptorHeap = std::get<3>(pipelinePresentResult);

    if (pipelineState == nullptr || commandList == nullptr || commandBundle == nullptr || srvDescriptorHeap == nullptr) {
        success = false;
    }

    vertexBufferResult = CreateVertexBufferForPresentation(d3d_device, rootSignature, commandQueue, commandList, commandBundle, srvDescriptorHeap, rtTexture);
    uploadDevHostBuffer = std::get<0>(vertexBufferResult);
    vertexBuffer = std::get<1>(vertexBufferResult);

    return std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, rtvDescriptorHeap, srvDescriptorHeap, uploadDevHostBuffer, vertexBuffer, rtTexture, success);
}

