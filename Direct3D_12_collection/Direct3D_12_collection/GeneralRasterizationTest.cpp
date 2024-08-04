#include "common.h"

#define BIND_DEPTH_STENCIL_AS_SRV   0
#define OUTPUT_DEPTH_TEXTURE        0
#define DO_BASIC_PRIMITIVE_TEST     0

static constexpr UINT TEXTURE_SIZE = WINDOW_WIDTH / 16;
static constexpr float PER_PIXEL_WIDTH = 2.0f / float(TEXTURE_SIZE);
static constexpr float HALF_PIXEL_WIDTH = PER_PIXEL_WIDTH * 0.5f;
static constexpr UINT TEXTURE_SAMPLE_COUNT = 1U;
static constexpr bool MSAA_RENDER_TARGET_NEED_RESOLVE = true && TEXTURE_SAMPLE_COUNT > 1U;
static constexpr UINT uavBufferSize = 64U;

enum CBV_SRV_UAV_SLOT_ID
{
    CBV_DRAW_INDEX_SLOT,
    UAV_PS_INVOKE_COUNT_SLOT,
    UAV_COMPUTE_OUTPUT_SLOT,
    SRV_DEPTH_TEXTURE_SLOT,
    SLOT_COUNT
};

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

// @return [rtvDescriptorHeap, dsvDescriptorHeap, rtTexture, dsTexture, resolvedRTTexture, resolvedDSTexutre]
static auto CreateRenderTargetViewForTexture(ID3D12Device* d3d_device) -> std::tuple<ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*>
{
    ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
    ID3D12DescriptorHeap* dsvDescriptorHeap = nullptr;
    ID3D12Resource* rtTexture = nullptr;
    ID3D12Resource* dsTexture = nullptr;
    ID3D12Resource* resolvedRTTexture = nullptr;
    ID3D12Resource* resolvedDSTexutre = nullptr;

    auto result = std::make_tuple(rtvDescriptorHeap, dsvDescriptorHeap, rtTexture, dsTexture, resolvedRTTexture, resolvedDSTexutre);

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

        return std::make_tuple(rtvDescriptorHeap, dsvDescriptorHeap, rtTexture, dsTexture, resolvedRTTexture, resolvedDSTexutre);
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
                                        std::tuple<ID3D12PipelineState*, ID3D12PipelineState*, ID3D12CommandAllocator*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12PipelineState* pointPipelineState = nullptr;
    ID3D12CommandAllocator* pointCommandBundleAllocator = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;
    ID3D12GraphicsCommandList* pointCommandBundle = nullptr;
    ID3D12DescriptorHeap* cbv_uavDescriptorHeap = nullptr;

    auto result = std::make_tuple(pipelineState, pointPipelineState, pointCommandBundleAllocator, commandList, commandBundleList, pointCommandBundle, cbv_uavDescriptorHeap);

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("cso/raster.vert.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("cso/raster.frag.cso");

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
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC trianglePSODesc {
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
                .MultisampleEnable = TEXTURE_SAMPLE_COUNT > 1U,
                .AntialiasedLineEnable = FALSE,
                .ForcedSampleCount = 0U,
                .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
            },
            .DepthStencilState {
                .DepthEnable = FALSE,
                .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
                .DepthFunc = D3D12_COMPARISON_FUNC_LESS,        // D3D12_COMPARISON_FUNC_NEVER
                .StencilEnable = FALSE,
                .StencilReadMask = 0U,
                .StencilWriteMask = 0U,
                .FrontFace { },
                .BackFace { }
            },
            .InputLayout {
                .pInputElementDescs = inputElementDescs,
                .NumElements = (UINT)std::size(inputElementDescs)
            },
            .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1U,
            .RTVFormats {
                // RTVFormats[0]
                { RENDER_TARGET_BUFFER_FOMRAT }
            },
            .DSVFormat = DXGI_FORMAT_UNKNOWN,
            .SampleDesc {
                .Count = TEXTURE_SAMPLE_COUNT,
                .Quality = 0U
            },
            .NodeMask = 0U,
            .CachedPSO { },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE
        };

        HRESULT hRes = d3d_device->CreateGraphicsPipelineState(&trianglePSODesc, IID_PPV_ARGS(&pipelineState));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateGraphicsPipelineState for PSO failed: %ld\n", hRes);
            break;
        }

#if DO_BASIC_PRIMITIVE_TEST
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pointPSODesc = trianglePSODesc;
        pointPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        hRes = d3d_device->CreateGraphicsPipelineState(&pointPSODesc, IID_PPV_ARGS(&pointPipelineState));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateGraphicsPipelineState for point PSO failed: %ld\n", hRes);
            break;
        }

        hRes = d3d_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&pointCommandBundleAllocator));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommandAllocator for point command bundle failed: %ld\n", hRes);
            break;
        }

        hRes = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, pointCommandBundleAllocator, pointPipelineState, IID_PPV_ARGS(&pointCommandBundle));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommandList for command bundle failed: %ld\n", hRes);
            break;
        }
#endif

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
            .NumDescriptors = CBV_SRV_UAV_SLOT_ID::SLOT_COUNT,      // This descriptor heap is for all of CBV, UAV and SRV buffers.
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0
        };
        hRes = d3d_device->CreateDescriptorHeap(&cbv_uavHeapDesc, IID_PPV_ARGS(&cbv_uavDescriptorHeap));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateDescriptorHeap for unordered access view failed: %ld\n", hRes);
            return result;
        }

        result = std::make_tuple(pipelineState, pointPipelineState, pointCommandBundleAllocator, commandList, commandBundleList, pointCommandBundle, cbv_uavDescriptorHeap);
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
                                                    std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;
    ID3D12DescriptorHeap* descriptorHeap = nullptr;

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("cso/cr_present.vert.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("cso/raster_present.frag.cso");

    do
    {
        if (vertexShaderObj.pShaderBytecode == nullptr || vertexShaderObj.BytecodeLength == 0) break;
        if (pixelShaderObj.pShaderBytecode == nullptr || pixelShaderObj.BytecodeLength == 0) break;

        // Define the vertex input layout used for Input Assembler
        const D3D12_INPUT_ELEMENT_DESC inputElementDescs[]{
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC trianglePSODesc{
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
                .StencilReadMask = 0U,
                .StencilWriteMask = 0U,
                .FrontFace { },
                .BackFace { }
            },
            .InputLayout {
                .pInputElementDescs = inputElementDescs,
                .NumElements = (UINT)std::size(inputElementDescs)
            },
            .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1U,
            .RTVFormats {
                // RTVFormats[0]
                { RENDER_TARGET_BUFFER_FOMRAT }
            },
            .DSVFormat = DXGI_FORMAT_UNKNOWN,
            .SampleDesc {
                .Count = USE_MSAA_RENDER_TARGET == 0 ? 1U : USE_MSAA_RENDER_TARGET,
                .Quality = 0U
            },
            .NodeMask = 0U,
            .CachedPSO { },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE
        };

        HRESULT hRes = d3d_device->CreateGraphicsPipelineState(&trianglePSODesc, IID_PPV_ARGS(&pipelineState));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateGraphicsPipelineState for triangle PSO failed: %ld\n", hRes);
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

// @return [uploadDevHostBuffer, vertexBuffer, indexBuffer, constantBuffer, uavBuffer, readbackDevHostBuffer, readBackTextureHostBuffer, uavCompOutBuffer]
static auto CreateVertexBufferForRenderTexture(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12CommandQueue *commandQueue, ID3D12GraphicsCommandList* commandList,
                                            ID3D12GraphicsCommandList* commandBundleList, ID3D12GraphicsCommandList* pointCommandBundle, ID3D12PipelineState* pointPipelineState, ID3D12DescriptorHeap* cbv_uavDescriptorHeap) ->
                                            std::tuple<ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*>
{
    const struct VertexInfo
    {
        float position[4];
        float color[4];
    } triangleVertices[]
    {
        // Direct3D是以左手作为前面背面顶点排列的依据
#if DO_BASIC_PRIMITIVE_TEST
        // first blue triangle
        {.position { -8.0f * PER_PIXEL_WIDTH, -8.0f * PER_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } },   // bottom-left
        {.position { -8.0f * PER_PIXEL_WIDTH, 8.0f * PER_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } },    // top-left
        {.position { 8.0f * PER_PIXEL_WIDTH, 8.0f * PER_PIXEL_WIDTH,  0.0f, 1.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } },    // top-right
        // second yellow triangle
        {.position { -8.0f * PER_PIXEL_WIDTH, -8.0f * PER_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.9f, 0.9f, 0.1f, 1.0f } },   // bottom-left
        {.position { 8.0f * PER_PIXEL_WIDTH, 8.0f * PER_PIXEL_WIDTH,  0.0f, 1.0f }, .color { 0.9f, 0.9f, 0.1f, 1.0f } },    // top-right
        {.position { 8.0f * PER_PIXEL_WIDTH, -8.0f * PER_PIXEL_WIDTH,  0.0f, 1.0f }, .color { 0.9f, 0.9f, 0.1f, 1.0f } },   // bottom-right
        // center point
        {.position { 0.0f, 0.0f, 0.0f, 1.0f}, .color { 0.1f, 0.1f, 0.1f, 1.0f } }
#else
        // first purple triangle
        { .position { -6.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 3.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.47f, 0.4f, 1.0f, 1.0f } },
        { .position { -8.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.47f, 0.4f, 1.0f, 1.0f } },
        { .position { -4.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.47f, 0.4f, 1.0f, 1.0f } },
        // second yellow triangle
        {.position { -6.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 3.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.85f, 0.84f, 0.42f, 1.0f } },
        {.position { -4.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.85f, 0.84f, 0.42f, 1.0f } },
        {.position { -2.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 2.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.85f, 0.84f, 0.42f, 1.0f } },
        // third green triangle
        {.position { -2.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 2.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
        {.position { -4.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
        {.position { 1.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
        // 4th purple triangle
        {.position { -2.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 2.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.47f, 0.4f, 1.0f, 1.0f } },
        {.position { 1.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.47f, 0.4f, 1.0f, 1.0f } },
        {.position { HALF_PIXEL_WIDTH, -HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.47f, 0.4f, 1.0f, 1.0f } },
        // 5th yellow triangle
        {.position { HALF_PIXEL_WIDTH, -HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.85f, 0.84f, 0.42f, 1.0f } },
        {.position { 1.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.85f, 0.84f, 0.42f, 1.0f } },
        {.position { 1.0f * PER_PIXEL_WIDTH + (1.0f + 0.5f) * HALF_PIXEL_WIDTH, 1.0f * PER_PIXEL_WIDTH + 0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.85f, 0.84f, 0.42f, 1.0f } },
        // 6th red triangle
        {.position { 1.0f * PER_PIXEL_WIDTH + (1.0f + 0.5f) * HALF_PIXEL_WIDTH, 1.0f * PER_PIXEL_WIDTH + 0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.97f, 0.4f, 0.42f, 1.0f } },
        {.position { 1.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.97f, 0.4f, 0.42f, 1.0f } },
        {.position { 7.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.97f, 0.4f, 0.42f, 1.0f } },

        // 7th red triangle
        {.position { -8.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.97f, 0.4f, 0.42f, 1.0f } },
        {.position { -8.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.97f, 0.4f, 0.42f, 1.0f } },
        {.position { -6.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 3.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.97f, 0.4f, 0.42f, 1.0f } },
        // 8th green triangle
        {.position { -8.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
        {.position { -6.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 3.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
        {.position { -5.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, -HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
        // 9th pink triangle
        {.position { -5.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, -HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.91f, 0.53f, 0.95f, 1.0f } },
        {.position { -6.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 3.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.91f, 0.53f, 0.95f, 1.0f } },
        {.position { -2.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 2.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.91f, 0.53f, 0.95f, 1.0f } },
        // 10th cyan triangle
        {.position { -5.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, -HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.42f, 0.82f, 1.0f, 1.0f } },
        {.position { -2.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 2.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.42f, 0.82f, 1.0f, 1.0f } },
        {.position { -2.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, -HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.42f, 0.82f, 1.0f, 1.0f } },
        // 11th red triangle
        {.position { -2.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, -HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.97f, 0.4f, 0.42f, 1.0f } },
        {.position { -2.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 2.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.97f, 0.4f, 0.42f, 1.0f } },
        {.position { HALF_PIXEL_WIDTH, -HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.97f, 0.4f, 0.42f, 1.0f } },
        // 12th brown triangle
        {.position { 1.0f * PER_PIXEL_WIDTH + 0.5f * HALF_PIXEL_WIDTH, -1.0f * PER_PIXEL_WIDTH + 0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.67f, 0.42f, 0.49f, 1.0f } },
        {.position { HALF_PIXEL_WIDTH, -HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.67f, 0.42f, 0.49f, 1.0f } },
        {.position { 1.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, -0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.67f, 0.42f, 0.49f, 1.0f } },
        // 13th pink triangle
        {.position { 1.0f * PER_PIXEL_WIDTH + 0.5f * HALF_PIXEL_WIDTH, -1.0f * PER_PIXEL_WIDTH + 0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.91f, 0.53f, 0.95f, 1.0f } },
        {.position { 1.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, -0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.91f, 0.53f, 0.95f, 1.0f } },
        {.position { 7.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, -3.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.91f, 0.53f, 0.95f, 1.0f } },
        // 14th yellow triangle
        {.position { 7.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, -3.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.85f, 0.84f, 0.42f, 1.0f } },
        {.position { 1.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, -0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.85f, 0.84f, 0.42f, 1.0f } },
        {.position { 3.0f * PER_PIXEL_WIDTH - 0.5f * HALF_PIXEL_WIDTH, 0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.85f, 0.84f, 0.42f, 1.0f } },
        // 15th green triangle
        {.position { 3.0f * PER_PIXEL_WIDTH - 0.5f * HALF_PIXEL_WIDTH, 0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
        {.position { 7.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
        {.position { 7.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, -3.0f * PER_PIXEL_WIDTH - HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
        // 16th cyan triangle
        {.position { 1.0f * PER_PIXEL_WIDTH + (1.0f + 0.5f) * HALF_PIXEL_WIDTH, 1.0f * PER_PIXEL_WIDTH + 0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.42f, 0.82f, 1.0f, 1.0f } },
        {.position { 7.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 5.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.42f, 0.82f, 1.0f, 1.0f } },
        {.position { 3.0f * PER_PIXEL_WIDTH - 0.5f * HALF_PIXEL_WIDTH, 0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.42f, 0.82f, 1.0f, 1.0f } },
        // 17th purple triangle
        {.position { 1.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, -0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.47f, 0.4f, 1.0f, 1.0f } },
        {.position { 1.0f * PER_PIXEL_WIDTH + (1.0f + 0.5f) * HALF_PIXEL_WIDTH, 1.0f * PER_PIXEL_WIDTH + 0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.47f, 0.4f, 1.0f, 1.0f } },
        {.position { 3.0f * PER_PIXEL_WIDTH - 0.5f * HALF_PIXEL_WIDTH, 0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.47f, 0.4f, 1.0f, 1.0f } },
        // 18th green triangle
        {.position { HALF_PIXEL_WIDTH, -HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
        {.position { 1.0f * PER_PIXEL_WIDTH + (1.0f + 0.5f) * HALF_PIXEL_WIDTH, 1.0f * PER_PIXEL_WIDTH + 0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
        {.position { 1.0f * PER_PIXEL_WIDTH + HALF_PIXEL_WIDTH, -0.5f * HALF_PIXEL_WIDTH, 0.0f, 1.0f }, .color { 0.48f, 0.88f, 0.42f, 1.0f } },
#endif
    };

    constexpr unsigned indexCount = 16;

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
    ID3D12Resource* readBackTextureHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* indexBuffer = nullptr;
    ID3D12Resource* constantBuffer = nullptr;
    ID3D12Resource* uavBuffer = nullptr;
    ID3D12Resource* uavCompOutBuffer = nullptr;

    auto result = std::make_tuple(uploadDevHostBuffer, vertexBuffer, indexBuffer, constantBuffer, uavBuffer, readbackDevHostBuffer, readBackTextureHostBuffer, uavCompOutBuffer);

    // Create vertexBuffer on GPU side.
    HRESULT hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vbResourceDesc,
                                                    D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&vertexBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for vertex buffer failed: %ld\n", hRes);
        return result;
    }

    // Create indexBuffer on GPU side.
    D3D12_RESOURCE_DESC ibResourceDesc = vbResourceDesc;
    ibResourceDesc.Width = indexCount * sizeof(unsigned);
    hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &ibResourceDesc,
                                                    D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&indexBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for index buffer failed: %ld\n", hRes);
        return result;
    }

    // Create uploadDevHostBuffer with host visible for upload
    D3D12_RESOURCE_DESC uploadResourceDesc = vbResourceDesc;
    uploadResourceDesc.Width += ibResourceDesc.Width + uavBufferSize;
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

    D3D12_CPU_DESCRIPTOR_HANDLE cbvCPUDescHandle = cbv_uavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE uavCPUDescHandle = cbvCPUDescHandle;
    cbvCPUDescHandle.ptr += CBV_DRAW_INDEX_SLOT * cbv_uavDescriptorSize;
    uavCPUDescHandle.ptr += UAV_PS_INVOKE_COUNT_SLOT * cbv_uavDescriptorSize;

    // Create the constant buffer view
    const D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{
        .BufferLocation = constantBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = CONSTANT_BUFFER_ALLOCATION_GRANULARITY
    };
    d3d_device->CreateConstantBufferView(&cbvDesc, cbvCPUDescHandle);

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
    memcpy(hostMemPtr, triangleVertices, sizeof(triangleVertices));

    // Assign index data
    unsigned* pIndices = (unsigned*)(uintptr_t(hostMemPtr) + sizeof(triangleVertices));
    for (unsigned i = 0U; i < indexCount; ++i) {
        pIndices[i] = i;
    }

    // Clear the UAV data
    memset(&pIndices[indexCount], 0, uavBufferSize);

    uploadDevHostBuffer->Unmap(0, nullptr);

    WriteToDeviceResourceAndSync(commandList, vertexBuffer, uploadDevHostBuffer, 0U, 0U, sizeof(triangleVertices));
    WriteToDeviceResourceAndSync(commandList, indexBuffer, uploadDevHostBuffer, 0U, sizeof(triangleVertices), indexCount * sizeof(unsigned));
    WriteToDeviceResourceAndSync(commandList, uavBuffer, uploadDevHostBuffer, 0U, sizeof(triangleVertices) + indexCount * sizeof(unsigned), uavBufferSize);

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
    pTranslations->rotAngle = -89.0f * 0.0f;
    pTranslations->zOffsetFront = -2.333f;    // -1.8 is the neareast plane for frustum perspective projection
    pTranslations->zOffsetBack = -2.8f;     // -2.0 is the near clipping plane and -2.333 is the far clipping plane for orthogonal projection

    constantBuffer->Unmap(0, nullptr);

    // Execute the command list to complete the copy operation
    ID3D12CommandList* const ppCommandLists[] = { (ID3D12CommandList*)commandList };
    commandQueue->ExecuteCommandLists((UINT)std::size(ppCommandLists), ppCommandLists);

    // Initialize the vertex buffer view.
    const D3D12_VERTEX_BUFFER_VIEW vertexBufferView{
        .BufferLocation = vertexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = UINT(sizeof(triangleVertices)),
        .StrideInBytes = sizeof(triangleVertices[0])
    };

    const D3D12_INDEX_BUFFER_VIEW indexBufferView{
        .BufferLocation = indexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = UINT(indexCount * sizeof(unsigned)),
        .Format = DXGI_FORMAT_R32_UINT
    };

    D3D12_GPU_DESCRIPTOR_HANDLE cbvGPUDescHandle = cbv_uavDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE uavGPUDescHandle = cbvGPUDescHandle;
    cbvGPUDescHandle.ptr += CBV_DRAW_INDEX_SLOT * cbv_uavDescriptorSize;
    uavGPUDescHandle.ptr += UAV_PS_INVOKE_COUNT_SLOT * cbv_uavDescriptorSize;

    ID3D12DescriptorHeap* const descHeaps[]{ cbv_uavDescriptorHeap };

    // Record commands to the command list bundle.
    commandBundleList->SetGraphicsRootSignature(rootSignature);
    // ATTENTION: SetDescriptorHeaps should be set into command bundle list as well as command list
    commandBundleList->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
    commandBundleList->SetGraphicsRootDescriptorTable(0U, cbvGPUDescHandle);     // rootParameters[0]
    commandBundleList->SetGraphicsRootDescriptorTable(1U, uavGPUDescHandle);     // rootParameters[1]
    commandBundleList->SetGraphicsRoot32BitConstant(2U, 0, 0);                   // rootParameters[2]
    commandBundleList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandBundleList->IASetVertexBuffers(0, 1, &vertexBufferView);

    constexpr UINT vertexCount = UINT(sizeof(triangleVertices) / sizeof(triangleVertices[0]));

#if DO_BASIC_PRIMITIVE_TEST
    commandBundleList->DrawInstanced(vertexCount - 1U, 1U, 0U, 0U);
#else
    commandBundleList->DrawInstanced(vertexCount, 1U, 0U, 0U);
#endif

    // End of the record
    hRes = commandBundleList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close basic command bundle failed: %ld\n", hRes);
        return result;
    }

    if (pointCommandBundle != nullptr && pointPipelineState != nullptr)
    {
        pointCommandBundle->SetGraphicsRootSignature(rootSignature);
        pointCommandBundle->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
        pointCommandBundle->SetGraphicsRootDescriptorTable(0U, cbvGPUDescHandle);     // rootParameters[0]
        pointCommandBundle->SetGraphicsRootDescriptorTable(1U, uavGPUDescHandle);     // rootParameters[1]
        pointCommandBundle->SetGraphicsRoot32BitConstant(2U, 0, 0);                   // rootParameters[2]
        pointCommandBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        pointCommandBundle->IASetVertexBuffers(0, 1, &vertexBufferView);

        pointCommandBundle->DrawInstanced(1U, 1U, vertexCount - 1U, 0U);

        // End of the record
        hRes = pointCommandBundle->Close();
        if (FAILED(hRes))
        {
            fprintf(stderr, "Close point command bundle failed: %ld\n", hRes);
            return result;
        }
    }

    // Wait for the command list to execute;
    // we are reusing the same command list in our main loop but for now,
    // we just want to wait for setup to complete before continuing.
    WaitForPreviousFrame(commandQueue);

    return std::make_tuple(uploadDevHostBuffer, vertexBuffer, indexBuffer, constantBuffer, uavBuffer, readbackDevHostBuffer, readBackTextureHostBuffer, uavCompOutBuffer);
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

static auto PopulateCommandList(ID3D12GraphicsCommandList* commandBundle, ID3D12GraphicsCommandList *pointCommandBundle, ID3D12GraphicsCommandList* commandList,
                                ID3D12DescriptorHeap* rtvDescriptorHeap, ID3D12DescriptorHeap* dsvDescriptorHeap,
                                ID3D12DescriptorHeap* cbv_uavDescriptorHeap, ID3D12QueryHeap* queryHeap,
                                ID3D12Resource* renderTarget, ID3D12Resource* dsTexture, ID3D12Resource* resolvedRTTexture, ID3D12Resource* resolvedDSTexture,
                                ID3D12Resource* uavBuffer, ID3D12Resource* readbackDevHostBuffer) -> bool
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

    // Insert the begin query
    commandList->BeginQuery(queryHeap, D3D12_QUERY_TYPE_OCCLUSION, 0U);

    // Execute the bundle to the command list
    commandList->ExecuteBundle(commandBundle);

    if (pointCommandBundle != nullptr) {
        commandList->ExecuteBundle(pointCommandBundle);
    }

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
            },
            // depth stencil state transition
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition {
                    .pResource = dsTexture,
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE,
                    .StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE
                }
            },
            // resolved depth stencil texture transition
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition {
                    .pResource = resolvedDSTexture,
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = D3D12_RESOURCE_STATE_COMMON,
                    .StateAfter = D3D12_RESOURCE_STATE_RESOLVE_DEST
                }
            }
        };
        commandList->ResourceBarrier((UINT)std::size(storeBarriers) - UINT(dsTexture == nullptr) * 2U, storeBarriers);

        // Resolve MSAA render target and depth stencil texture to resovled textures
        commandList->ResolveSubresource(resolvedRTTexture, 0, renderTarget, 0, RENDER_TARGET_BUFFER_FOMRAT);
        if (dsTexture != nullptr && resolvedDSTexture != nullptr) {
            commandList->ResolveSubresource(resolvedDSTexture, 0, dsTexture, 0, DXGI_FORMAT_R32_FLOAT);
        }

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
            },
            // resolved depth stencil texture transition
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition {
                    .pResource = resolvedDSTexture,
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST,
                    .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                }
            }
        };
        commandList->ResourceBarrier((UINT)std::size(resolvedBarriers) - UINT(dsTexture == nullptr), resolvedBarriers);
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

#if OUTPUT_DEPTH_TEXTURE
static auto CreateRootSignatureForCompute(ID3D12Device* d3d_device) -> ID3D12RootSignature*
{
    ID3D12RootSignature* rootSignature = nullptr;

    const D3D12_DESCRIPTOR_RANGE descRanges[]{
        // t0 (for depth texture)
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        },
        // u0 (for destination buffer)
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        }
    };

    const D3D12_ROOT_PARAMETER rootParameters[]{
        // t0
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[0]
            },
            // This texture buffer will just be accessed in a compute shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        // u0
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[1]
            },
            // This unordered access view buffer will just be accessed in a compute shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        // b0
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants {
                .ShaderRegister = 0,
                .RegisterSpace = 0,
                .Num32BitValues = 1
            },
            // This unordered access view buffer will just be accessed in a compute shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        }
    };

    // Create a root signature.
    const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{
        .NumParameters = (UINT)std::size(rootParameters),
        .pParameters = rootParameters,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
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

// @return [pipelineState, commandList, commandBundle]
static auto CreatePipelineStateObjectForCompute(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature,
                                                ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                                std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;

    D3D12_SHADER_BYTECODE computeShaderObj = CreateCompiledShaderObjectFromPath("cso/cr.comp.cso");

    do
    {
        if (computeShaderObj.pShaderBytecode == nullptr || computeShaderObj.BytecodeLength == 0) break;

        const D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc{
            .pRootSignature = rootSignature,
            .CS = computeShaderObj,
            .NodeMask = 0,
            .CachedPSO { nullptr, 0U },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE
        };
        HRESULT hRes = d3d_device->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(&pipelineState));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateComputePipelineState for PSO failed: %ld\n", hRes);
            break;
        }

        hRes = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, pipelineState, IID_PPV_ARGS(&commandList));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommandList for basic PSO failed: %ld\n", hRes);
            break;
        }

        hRes = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, commandBundleAllocator, pipelineState, IID_PPV_ARGS(&commandBundle));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommandList for command bundle failed: %ld\n", hRes);
            break;
        }
    }
    while (false);

    if (computeShaderObj.pShaderBytecode != nullptr) {
        free((void*)computeShaderObj.pShaderBytecode);
    }

    return std::make_tuple(pipelineState, commandList, commandBundle);
}

static auto PopulateComputeCommandList(ID3D12Device* d3d_device, ID3D12PipelineState* computePipelineState, ID3D12RootSignature* computeRootSignature,
    ID3D12GraphicsCommandList* commandList, ID3D12GraphicsCommandList* commandBundle, ID3D12DescriptorHeap* cbv_uavDescriptorHeap,
    ID3D12Resource* dsTexture, ID3D12Resource* resolvedDSTexture,
    ID3D12Resource* readBackTextureBuffer, ID3D12Resource* computeOutBuffer) -> bool
{
    if (computePipelineState == nullptr || (resolvedDSTexture == nullptr && dsTexture == nullptr)) return false;

    constexpr bool isMSAA = !MSAA_RENDER_TARGET_NEED_RESOLVE && TEXTURE_SAMPLE_COUNT > 1;
    const UINT cbv_uavDescriptorSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cbvCPUDescHandle = cbv_uavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE uavCPUDescHandle = cbvCPUDescHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE srvCPUDescHandle = cbvCPUDescHandle;
    uavCPUDescHandle.ptr += UAV_COMPUTE_OUTPUT_SLOT * cbv_uavDescriptorSize;
    srvCPUDescHandle.ptr += SRV_DEPTH_TEXTURE_SLOT * cbv_uavDescriptorSize;

    // Create the unordered access buffer view
    const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer {
            .FirstElement = 0,
            .NumElements = TEXTURE_SIZE * TEXTURE_SIZE,
            .StructureByteStride = UINT(sizeof(float)),
            .CounterOffsetInBytes = 0,
            .Flags = D3D12_BUFFER_UAV_FLAG_NONE
        }
    };
    d3d_device->CreateUnorderedAccessView(computeOutBuffer, nullptr, &uavDesc, uavCPUDescHandle);

    // Create the texture shader resource view
    const D3D12_SHADER_RESOURCE_VIEW_DESC textureSRVDesc{
        .Format = DXGI_FORMAT_R32_FLOAT,
        .ViewDimension = isMSAA ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D {
            .MostDetailedMip = 0,
            .MipLevels = 1,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0f
        }
    };
    d3d_device->CreateShaderResourceView(MSAA_RENDER_TARGET_NEED_RESOLVE ? resolvedDSTexture : dsTexture, &textureSRVDesc, srvCPUDescHandle);

    D3D12_GPU_DESCRIPTOR_HANDLE cbvGPUDescHandle = cbv_uavDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE uavGPUDescHandle = cbvGPUDescHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGPUDescHandle = cbvGPUDescHandle;
    uavGPUDescHandle.ptr += UAV_COMPUTE_OUTPUT_SLOT * cbv_uavDescriptorSize;
    srvGPUDescHandle.ptr += SRV_DEPTH_TEXTURE_SLOT * cbv_uavDescriptorSize;

    // This setting is optional because the initial state of this command list is computePipelineState
    commandList->SetPipelineState(computePipelineState);

    ID3D12DescriptorHeap* const descHeaps[]{ cbv_uavDescriptorHeap };
    commandList->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);

    commandBundle->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
    commandBundle->SetComputeRootSignature(computeRootSignature);
    commandBundle->SetComputeRootDescriptorTable(0, srvGPUDescHandle);
    commandBundle->SetComputeRootDescriptorTable(1, uavGPUDescHandle);
    commandBundle->SetComputeRoot32BitConstant(2U, 2U, 0U);   // Set sample index
    commandBundle->Dispatch(TEXTURE_SIZE / 16U, TEXTURE_SIZE / 16U, 1U);

    auto hRes = commandBundle->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close compute command bundle failed: %ld\n", hRes);
        return false;
    }

    commandList->ExecuteBundle(commandBundle);

    SyncAndReadFromDeviceResource(commandList, TEXTURE_SIZE * TEXTURE_SIZE * sizeof(float), readBackTextureBuffer, computeOutBuffer);

    hRes = commandList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close compute command bundle failed: %ld\n", hRes);
        return false;
    }

    return true;
}
#endif

auto CreateGeneralRasterizationTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue *commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                    std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>
{
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12RootSignature* computeRootSignature = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12PipelineState* pointPipelineState = nullptr;
    ID3D12PipelineState* computePipelineState = nullptr;
    ID3D12CommandAllocator* pointCommandBundleAllocator = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;
    ID3D12GraphicsCommandList* pointCommandBundle = nullptr;
    ID3D12GraphicsCommandList* computeCommandList = nullptr;
    ID3D12GraphicsCommandList* computeCommandBundle = nullptr;
    ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
    ID3D12DescriptorHeap* dsvDescriptorHeap = nullptr;
    ID3D12DescriptorHeap* cbv_uavDescriptorHeap = nullptr;
    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* indexBuffer = nullptr;
    ID3D12Resource* rtTexture = nullptr;
    ID3D12Resource* dsTexture = nullptr;
    ID3D12Resource* resolvedRTTexture = nullptr;
    ID3D12Resource* resolvedDSTexture = nullptr;
    ID3D12Resource* constantBuffer = nullptr;
    ID3D12Resource* uavBuffer = nullptr;
    ID3D12Resource* uavCompOutBuffer = nullptr;
    ID3D12Resource* readbackDevHostBuffer = nullptr;
    ID3D12Resource* readBackTextureHostBuffer = nullptr;
    ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;
    bool success = false;

    auto result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, rtvDescriptorHeap, srvDescriptorHeap, uploadDevHostBuffer, vertexBuffer, rtTexture, success);

    rootSignature = CreateRootSignature(d3d_device, true);
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
    dsvDescriptorHeap = std::get<1>(rtTexRes);
    rtTexture = std::get<2>(rtTexRes);
    dsTexture = std::get<3>(rtTexRes);
    resolvedRTTexture = std::get<4>(rtTexRes);
    resolvedDSTexture = std::get<5>(rtTexRes);

    auto const pipelineResult = CreatePipelineStateObjectForRenderTexture(d3d_device, commandAllocator, commandBundleAllocator, rootSignature);
    pipelineState = std::get<0>(pipelineResult);
    pointPipelineState = std::get<1>(pipelineResult);
    pointCommandBundleAllocator = std::get<2>(pipelineResult);
    commandList = std::get<3>(pipelineResult);
    commandBundle = std::get<4>(pipelineResult);
    pointCommandBundle = std::get<5>(pipelineResult);
    cbv_uavDescriptorHeap = std::get<6>(pipelineResult);

    auto const renderVertexBufferResult = CreateVertexBufferForRenderTexture(d3d_device, rootSignature, commandQueue, commandList, commandBundle, pointCommandBundle, pointPipelineState, cbv_uavDescriptorHeap);
    uploadDevHostBuffer = std::get<0>(renderVertexBufferResult);
    vertexBuffer = std::get<1>(renderVertexBufferResult);
    indexBuffer = std::get<2>(renderVertexBufferResult);
    constantBuffer = std::get<3>(renderVertexBufferResult);
    uavBuffer = std::get<4>(renderVertexBufferResult);
    readbackDevHostBuffer = std::get<5>(renderVertexBufferResult);
    readBackTextureHostBuffer = std::get<6>(renderVertexBufferResult);
    uavCompOutBuffer = std::get<7>(renderVertexBufferResult);

    do
    {
        if (!ResetCommandAllocatorAndList(commandAllocator, commandList, pipelineState)) break;

        if (!PopulateCommandList(commandBundle, pointCommandBundle, commandList, rtvDescriptorHeap, dsvDescriptorHeap, cbv_uavDescriptorHeap, queryHeap,
                                rtTexture, dsTexture, resolvedRTTexture, resolvedDSTexture, uavBuffer, readbackDevHostBuffer)) break;

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
        printf("Current Occlusion Query result: %llu\n", queryPtr[4]);

        readbackDevHostBuffer->Unmap(0, nullptr);

#if OUTPUT_DEPTH_TEXTURE
        computeRootSignature = CreateRootSignatureForCompute(d3d_device);
        if (computeRootSignature != nullptr)
        {
            auto const result = CreatePipelineStateObjectForCompute(d3d_device, computeRootSignature, commandAllocator, commandBundleAllocator);
            computePipelineState = std::get<0>(result);
            computeCommandList = std::get<1>(result);
            computeCommandBundle = std::get<2>(result);
        }

        if (!PopulateComputeCommandList(d3d_device, computePipelineState, computeRootSignature, computeCommandList, computeCommandBundle,
                                        cbv_uavDescriptorHeap, dsTexture, resolvedDSTexture, readBackTextureHostBuffer, uavCompOutBuffer)) break;

        ID3D12CommandList* const computeCommandLists[] = { (ID3D12CommandList*)computeCommandList };
        commandQueue->ExecuteCommandLists((UINT)std::size(computeCommandLists), computeCommandLists);

        if (!WaitForPreviousFrame(commandQueue)) break;

        float* texelPtr = nullptr;
        hRes = readBackTextureHostBuffer->Map(0, nullptr, (void**)&texelPtr);
        if (FAILED(hRes))
        {
            fprintf(stderr, "Map read back buffer failed: %ld\n", hRes);
            break;
        }

        constexpr auto quarterCount = TEXTURE_SIZE / 4;
        for (auto row = quarterCount; row < TEXTURE_SIZE - quarterCount; ++row)
        {
            for (auto col = quarterCount; col < TEXTURE_SIZE - quarterCount; ++col) {
                printf("%.3f  ", texelPtr[row * TEXTURE_SIZE + col]);
            }
            puts("");
        }

        readBackTextureHostBuffer->Unmap(0, nullptr);
#endif

        success = true;
    }
    while (false);

    result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, rtvDescriptorHeap, srvDescriptorHeap, uploadDevHostBuffer, vertexBuffer, rtTexture, success);

    if (!success) return result;

    rootSignature->Release();
    rootSignature = nullptr;

    pipelineState->Release();
    pipelineState = nullptr;

    queryHeap->Release();

    if (pointPipelineState != nullptr) {
        pointPipelineState->Release();
    }

    cbv_uavDescriptorHeap->Release();
    constantBuffer->Release();
    uavBuffer->Release();
    uavCompOutBuffer->Release();
    readbackDevHostBuffer->Release();
    readBackTextureHostBuffer->Release();
    indexBuffer->Release();

    if (computeRootSignature != nullptr) {
        computeRootSignature->Release();
    }
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
        if (dsTexture != nullptr) {
            dsTexture->Release();
        }

        rtTexture = resolvedRTTexture;
        dsTexture = resolvedDSTexture;
    }
    else
    {
        if (resolvedRTTexture != nullptr) {
            resolvedRTTexture->Release();
        }
        if (resolvedDSTexture != nullptr) {
            resolvedDSTexture->Release();
        }
    }
    
    commandList->Release();
    commandList = nullptr;

    commandBundle->Release();
    commandBundle = nullptr;

    if (pointCommandBundle != nullptr) {
        pointCommandBundle->Release();
    }

    if (pointCommandBundleAllocator != nullptr) {
        pointCommandBundleAllocator->Release();
    }

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

