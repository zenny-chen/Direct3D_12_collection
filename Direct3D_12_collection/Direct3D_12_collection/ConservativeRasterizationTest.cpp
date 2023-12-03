#include "common.h"
#include <array>

#define TEST_UNCERTAINTY_REGION     0
#define TEST_EARLY_DEPTH_CULLING    0
#define TEST_VARIABLE_SHADING_RATE  1
#define BIND_DEPTH_STENCIL_AS_SRV   0
#define TEST_PRIMITIVE_POINT        0

static constexpr D3D12_FILL_MODE USE_FILL_MODE = D3D12_FILL_MODE_SOLID;
static constexpr D3D12_CONSERVATIVE_RASTERIZATION_MODE USE_CONSERVATIVE_RASTERIZATION_MODE = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
static constexpr D3D_PRIMITIVE_TOPOLOGY RENDER_TEXTURE_USE_PRIMITIVE_TOPOLOGY = TEST_PRIMITIVE_POINT  != 0 ? D3D_PRIMITIVE_TOPOLOGY_POINTLIST : D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

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
constexpr UINT uavBufferSize = 16U;

static auto CreateRootSignature(ID3D12Device* d3d_device, bool isForRenderTexture) -> ID3D12RootSignature*
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
        // b0 (for render target texture rendering)
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
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
            // This texture buffer will just be accessed in a pixel shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
        },
        // b0 (for render target texture rendering)
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[1]
            },
            // This constant buffer view buffer will just be accessed in a vertex shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
        },
        // u0 (for render target texture rendering)
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[2]
            },
            // This unordered access view buffer will just be accessed in a pixel shader
            .ShaderVisibility =  D3D12_SHADER_VISIBILITY_PIXEL
        },
        // b1 (for render target texture rendering)
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants {
                .ShaderRegister = 1,
                .RegisterSpace = 0,
                .Num32BitValues = 1
            },
            // This unordered access view buffer will just be accessed in a pixel shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
        }
    };

    // Create a root signature.
    const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {
        .NumParameters = isForRenderTexture ? 3U : 1U,
        .pParameters = isForRenderTexture ? &rootParameters[1] : &rootParameters[0],
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

// @return [rtvDescriptorHeap, dsvDescriptorHeap, rtTexture, dsTexture]
static auto CreateRenderTargetViewForTexture(ID3D12Device* d3d_device) -> std::tuple<ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*>
{
    ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
    ID3D12DescriptorHeap* dsvDescriptorHeap = nullptr;
    ID3D12Resource* rtTexture = nullptr;
    ID3D12Resource* dsTexture = nullptr;

    auto result = std::make_tuple(rtvDescriptorHeap, dsvDescriptorHeap, rtTexture, dsTexture);

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

#if TEST_EARLY_DEPTH_CULLING
    // Describe and create a depth stencil view (DSV) descriptor heap.
    const D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0
    };
    hRes = d3d_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvDescriptorHeap));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateDescriptorHeap for depth stencil view failed: %ld\n", hRes);
        return result;
    }
#endif

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
        // Create the RTV texture
        hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &rtResourceDesc, D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                                                &rtOptClearValue, IID_PPV_ARGS(&rtTexture));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommittedResource for texture render target failed: %ld!\n", hRes);
            break;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        d3d_device->CreateRenderTargetView(rtTexture, &rtvDesc, rtvHandle);

        const UINT rtvDescriptorSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        rtvHandle.ptr += rtvDescriptorSize;

#if TEST_EARLY_DEPTH_CULLING

        const D3D12_RESOURCE_DESC dsResourceDesc{
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0,
            .Width = TEXTURE_SIZE,
            .Height = TEXTURE_SIZE,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc {.Count = TEXTURE_SAMPLE_COUNT, .Quality = 0U },
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        };

        const D3D12_CLEAR_VALUE dsOptClearValue{
            .Format = DXGI_FORMAT_D32_FLOAT,
            .DepthStencil { .Depth = 1.0f, .Stencil = 0U }
        };

        hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &dsResourceDesc, D3D12_RESOURCE_STATE_DEPTH_READ,
                                                    &dsOptClearValue, IID_PPV_ARGS(&dsTexture));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommittedResource for texture render target failed: %ld!\n", hRes);
            break;
        }

        const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
            .Format = DXGI_FORMAT_D32_FLOAT,
            .ViewDimension = TEXTURE_SAMPLE_COUNT > 1U ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D,
            .Flags = D3D12_DSV_FLAG_NONE,
            .Texture2D { .MipSlice = 0 }
        };

        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        d3d_device->CreateDepthStencilView(dsTexture, &dsvDesc, dsvHandle);

        const UINT dsvDescriptorSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        dsvHandle.ptr += dsvDescriptorSize;

#endif

        return std::make_tuple(rtvDescriptorHeap, dsvDescriptorHeap, rtTexture, dsTexture);
    }
    while (false);

    if (rtvDescriptorHeap != nullptr) {
        rtvDescriptorHeap->Release();
    }
    if (dsvDescriptorHeap != nullptr) {
        dsvDescriptorHeap->Release();
    }
    if (rtTexture != nullptr) {
        rtTexture->Release();
    }
    if (dsTexture != nullptr) {
        dsTexture->Release();
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
                .DepthEnable = TEST_EARLY_DEPTH_CULLING != 0,
                .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
                .DepthFunc = D3D12_COMPARISON_FUNC_LESS,        // D3D12_COMPARISON_FUNC_NEVER
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
            .PrimitiveTopologyType = RENDER_TEXTURE_USE_PRIMITIVE_TOPOLOGY_TYPE,
            .NumRenderTargets = 1,
            .RTVFormats {
                // RTVFormats[0]
                { RENDER_TARGET_BUFFER_FOMRAT }
            },
            .DSVFormat = DXGI_FORMAT_D32_FLOAT,
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
            .NumDescriptors = 2U,       // This descriptor heap is for both of CBV and UAV buffers.
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

// @return [uploadDevHostBuffer, vertexBuffer, constantBuffer, uavBuffer, readbackDevHostBuffer]
static auto CreateVertexBufferForRenderTexture(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12CommandQueue *commandQueue, ID3D12GraphicsCommandList* commandList,
                                            ID3D12GraphicsCommandList* commandBundleList, ID3D12PipelineState* linePipelineState, ID3D12DescriptorHeap* cbv_uavDescriptorHeap) -> std::tuple<ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*>
{
#if TEST_PRIMITIVE_POINT
    struct Vertex
    {
        float position[4];
        float color[4];
    } triangleVertices[256]
#else
    const struct Vertex
    {
        float position[4];
        float color[4];
    } triangleVertices[]
#endif
    {
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
#elif TEST_VARIABLE_SHADING_RATE
        {.position { -0.25f, 0.25f, 0.0f, 1.0f }, .color { 0.9f, 0.1f, 0.1f, 1.0f } },  // top left
        {.position { 0.25f, 0.25f, 0.0f, 1.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },   // top right
        {.position { -0.25f, -0.25f, 0.0f, 1.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } }, // bottom left
        {.position { 0.25f, -0.25f, 0.0f, 1.0f }, .color { 0.9f, 0.9f, 0.1f, 1.0f } }   // bottom right
#else
#if TEST_EARLY_DEPTH_CULLING
        {.position { 0.0f, 0.5f, 0.0f, 1.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },     // top center
        {.position { 0.7f, -0.5f, 0.0f, 1.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } },    // bottom right
        {.position { -0.7f, -0.5f, 0.0f, 1.0f }, .color { 0.9f, 0.1f, 0.1f, 1.0f } },   // bottom left
#endif // TEST_EARLY_DEPTH_CULLING
        {.position { 0.0f, 0.75f, 0.0f, 1.0f }, .color { 0.9f, 0.1f, 0.1f, 1.0f } },    // top center
        {.position { 0.75f, -0.75f, 0.0f, 1.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },  // bottom right
        {.position { -0.75f, -0.75f, 0.0f, 1.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } }  // bottom left
#endif
    };

#if TEST_PRIMITIVE_POINT
    const float pointColors[4 * 4] = {
        0.9f, 0.1f, 0.1f, 1.0f,     // pixel 0
        0.1f, 0.9f, 0.1f, 1.0f,     // pixel 1
        0.1f, 0.1f, 0.9f, 1.0f,     // pixel 2
        0.9f, 0.9f, 0.1f, 1.0f      // pixel 3
    };
    constexpr int regionSize = TEXTURE_SAMPLE_COUNT == 1U ? 16 : 8;
    constexpr float widthPerPixel = 2.0f / float(TEXTURE_SIZE);

    for (int y = 0; y < regionSize; ++y)
    {
        const float yCoord = -0.25f +  float(y) * (1.0f / 32.0f);
        const int pointColorYIndex = y & 3;

        for (int x = 0; x < regionSize; ++x)
        {
            const float xCoord = -0.25f + float(x) * (1.0f / 32.0f);
            const int pointColorXIndex = ((pointColorYIndex + x) & 3) * 4;
            int index = y * regionSize + x;
            
            triangleVertices[index].position[0] = xCoord;
            triangleVertices[index].position[1] = yCoord;
            triangleVertices[index].position[2] = 0.0f;
            triangleVertices[index].position[3] = 1.0f;

            memcpy(triangleVertices[index].color, &pointColors[pointColorXIndex], sizeof(triangleVertices[index].color));
        }
    }
#endif

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
    ID3D12Resource* readbackDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* constantBuffer = nullptr;
    ID3D12Resource* uavBuffer = nullptr;

    auto result = std::make_tuple(uploadDevHostBuffer, vertexBuffer, constantBuffer, uavBuffer, readbackDevHostBuffer);

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

    const UINT cbv_uavDescriptorSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cbvCPUDescHandle = cbv_uavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    // Create the constant buffer view
    const D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{
        .BufferLocation = constantBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = CONSTANT_BUFFER_ALLOCATION_GRANULARITY
    };
    d3d_device->CreateConstantBufferView(&cbvDesc, cbvCPUDescHandle);

    D3D12_CPU_DESCRIPTOR_HANDLE uavCPUDescHandle = cbvCPUDescHandle;
    uavCPUDescHandle.ptr += cbv_uavDescriptorSize;

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

    memset(hostMemPtr, 0, CONSTANT_BUFFER_ALLOCATION_GRANULARITY);
    // Set translations
    struct { float rotAngle; float zOffsetFront; float zOffsetBack; } *pTranslations;
    pTranslations = (decltype(pTranslations))hostMemPtr;
    pTranslations->rotAngle = 0.0f;
    pTranslations->zOffsetFront = -2.3f;    // -1.8 will cull the front triangle when using frustum perspective projection
    pTranslations->zOffsetBack = -2.325f;

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

    D3D12_GPU_DESCRIPTOR_HANDLE cbvGPUDescHandle = cbv_uavDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE uavGPUDescHandle = cbvGPUDescHandle;
    uavGPUDescHandle.ptr += cbv_uavDescriptorSize;

    ID3D12DescriptorHeap* const descHeaps[]{ cbv_uavDescriptorHeap };

    // Record commands to the command list bundle.
    commandBundleList->SetGraphicsRootSignature(rootSignature);
    // ATTENTION: SetDescriptorHeaps should be set into command bundle list as well as command list
    commandBundleList->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
    commandBundleList->SetGraphicsRootDescriptorTable(0U, cbvGPUDescHandle);     // rootParameters[0]
    commandBundleList->SetGraphicsRootDescriptorTable(1U, uavGPUDescHandle);     // rootParameters[1]
    commandBundleList->SetGraphicsRoot32BitConstant(2U, 0, 0);                   // rootParameters[2]
    commandBundleList->IASetPrimitiveTopology(RENDER_TEXTURE_USE_PRIMITIVE_TOPOLOGY);
    commandBundleList->IASetVertexBuffers(0, 1, &vertexBufferView);

#if TEST_VARIABLE_SHADING_RATE
    const D3D12_SHADING_RATE_COMBINER combiners[D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT] = { D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_PASSTHROUGH };
    ((ID3D12GraphicsCommandList5*)commandBundleList)->RSSetShadingRate(D3D12_SHADING_RATE_2X2, combiners);
    commandBundleList->DrawInstanced((UINT)std::size(triangleVertices), 1U, 0U, 0U);
#else
    commandBundleList->DrawInstanced(3U, 1U, 0U, 0U);
#endif

#if TEST_EARLY_DEPTH_CULLING
    commandBundleList->SetGraphicsRoot32BitConstant(2U, 1U, 0U);                // rootParameters[2]
    commandBundleList->DrawInstanced(3U, 1U, 3U, 0U);
#endif

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

    return std::make_tuple(uploadDevHostBuffer, vertexBuffer, constantBuffer, uavBuffer, readbackDevHostBuffer);
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

    // Fetch CBV and UAV CPU descriptor handles
    D3D12_CPU_DESCRIPTOR_HANDLE textureSRVCPUDescHandle = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    // Create the texture shader resource view
    const D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc{
        .Format = BIND_DEPTH_STENCIL_AS_SRV == 0 ? RENDER_TARGET_BUFFER_FOMRAT : DXGI_FORMAT_R32_FLOAT,
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

static auto PopulateCommandList(ID3D12Device* d3d_device, ID3D12GraphicsCommandList* commandBundle, ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* rtvDescriptorHeap, ID3D12DescriptorHeap* dsvDescriptorHeap,
                                ID3D12DescriptorHeap* cbv_uavDescriptorHeap, ID3D12Resource* renderTarget, ID3D12Resource* dsTexture, ID3D12Resource* uavBuffer, ID3D12Resource* readbackDevHostBuffer) -> bool
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

    // Indicate that the back buffer will be used as a render target.
    const D3D12_RESOURCE_BARRIER renderBarriers[] {
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
        },
        // depth stencil state transition
        {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition {
                .pResource = dsTexture,
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_DEPTH_READ,
                .StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE
            }
        }
    };
    commandList->ResourceBarrier((UINT)std::size(renderBarriers) - UINT(dsTexture == nullptr), renderBarriers);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
    if (dsvDescriptorHeap != nullptr) {
        dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    }
    
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, dsvDescriptorHeap != nullptr ? &dsvHandle : nullptr);

    const float clearColor[] = { 0.5f, 0.6f, 0.5f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    if (dsvDescriptorHeap != nullptr) {
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

    ID3D12DescriptorHeap* const descHeaps[]{ cbv_uavDescriptorHeap };
    commandList->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);

    // Execute the bundle to the command list
    commandList->ExecuteBundle(commandBundle);

    // Indicate that the back buffer will now be used to present.
    const D3D12_RESOURCE_BARRIER storeBarriers[] {
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
        },
        // depth stencil state transition
        {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition {
                .pResource = dsTexture,
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE,
                .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            }
        }
    };
    commandList->ResourceBarrier((UINT)std::size(storeBarriers) - UINT(dsTexture == nullptr), storeBarriers);

    SyncAndReadFromDeviceResource(commandList, uavBufferSize, readbackDevHostBuffer, uavBuffer);

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
    ID3D12DescriptorHeap* dsvDescriptorHeap = nullptr;
    ID3D12DescriptorHeap* cbv_uavDescriptorHeap = nullptr;
    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* rtTexture = nullptr;
    ID3D12Resource* dsTexture = nullptr;
    ID3D12Resource* constantBuffer = nullptr;
    ID3D12Resource* uavBuffer = nullptr;
    ID3D12Resource* readbackDevHostBuffer = nullptr;
    ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;
    bool success = false;

    auto result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, rtvDescriptorHeap, srvDescriptorHeap, uploadDevHostBuffer, vertexBuffer, rtTexture, success);

    rootSignature = CreateRootSignature(d3d_device, true);
    if (rootSignature == nullptr) return result;

    auto const rtTexRes = CreateRenderTargetViewForTexture(d3d_device);
    rtvDescriptorHeap = std::get<0>(rtTexRes);
    dsvDescriptorHeap = std::get<1>(rtTexRes);
    rtTexture = std::get<2>(rtTexRes);
    dsTexture = std::get<3>(rtTexRes);

    auto const pipelineResult = CreatePipelineStateObjectForRenderTexture(d3d_device, commandAllocator, commandBundleAllocator, rootSignature);
    pipelineState = std::get<0>(pipelineResult);
    commandList = std::get<1>(pipelineResult);
    commandBundle = std::get<2>(pipelineResult);
    cbv_uavDescriptorHeap = std::get<3>(pipelineResult);

    ID3D12PipelineState* linePipelineState = nullptr;

#if TEST_UNCERTAINTY_REGION
    linePipelineState = CreatePipelineStateObjectForLinesRenderTexture(d3d_device, rootSignature);
#endif

    auto const renderVertexBufferResult = CreateVertexBufferForRenderTexture(d3d_device, rootSignature, commandQueue, commandList, commandBundle, linePipelineState, cbv_uavDescriptorHeap);
    uploadDevHostBuffer = std::get<0>(renderVertexBufferResult);
    vertexBuffer = std::get<1>(renderVertexBufferResult);
    constantBuffer = std::get<2>(renderVertexBufferResult);
    uavBuffer = std::get<3>(renderVertexBufferResult);
    readbackDevHostBuffer = std::get<4>(renderVertexBufferResult);

    do
    {
        if (!ResetCommandAllocatorAndList(commandAllocator, commandList, pipelineState)) break;

        if (!PopulateCommandList(d3d_device, commandBundle, commandList, rtvDescriptorHeap, dsvDescriptorHeap, cbv_uavDescriptorHeap, rtTexture, dsTexture, uavBuffer, readbackDevHostBuffer)) break;

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

        printf("Current pixle shader invocation count: %u\n", *hostMemPtr);

        readbackDevHostBuffer->Unmap(0, nullptr);

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

    cbv_uavDescriptorHeap->Release();
    constantBuffer->Release();
    uavBuffer->Release();
    readbackDevHostBuffer->Release();
    
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

    auto const presentVertexBufferResult = CreateVertexBufferForPresentation(d3d_device, rootSignature, commandQueue, commandList, commandBundle, srvDescriptorHeap, BIND_DEPTH_STENCIL_AS_SRV == 0 ? rtTexture : dsTexture);
    uploadDevHostBuffer = presentVertexBufferResult.first;
    vertexBuffer = presentVertexBufferResult.second;

#if BIND_DEPTH_STENCIL_AS_SRV
    rtvDescriptorHeap->Release();
    rtTexture->Release();
#else
    if (dsvDescriptorHeap != nullptr) {
        dsvDescriptorHeap->Release();
    }
    if (dsTexture != nullptr) {
        dsTexture->Release();
    }
#endif

    return std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, BIND_DEPTH_STENCIL_AS_SRV == 0 ? rtvDescriptorHeap : dsvDescriptorHeap,
                            srvDescriptorHeap, uploadDevHostBuffer, vertexBuffer, BIND_DEPTH_STENCIL_AS_SRV == 0 ? rtTexture : dsTexture, success);
}

