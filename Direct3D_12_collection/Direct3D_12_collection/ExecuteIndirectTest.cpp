#include "common.h"


enum CBV_SRV_UAV_SLOT_ID
{
    INDIRECT_ARGUMENT_BUFFER_UAV_SLOT,
    INDIRECT_COUNT_BUFFER_UAV_SLOT,
    DRAW_COMMAND_ROTATE_CBV_SLOT,
    CBV_SRV_UAV_SLOT_COUNT
};

// sizeof(IndirectArgumentBufferType) MUST BE at least 32 bytes.
struct IndirectArgumentBufferType
{
    unsigned VertexCountPerInstance_IndexCountPerInstance_ThreadGroupX;
    unsigned InstanceCount_ThreadGroupCountY;
    unsigned StartVertexLocation_StartIndexLocation_ThreadGroupCountZ;
    unsigned StartInstanceLocation_BaseVertexLocation;
    unsigned StartInstanceLocation;
    unsigned paddings[3];
};

static auto CreateRootSignature(ID3D12Device* d3d_device) -> ID3D12RootSignature*
{
    ID3D12RootSignature* rootSignature = nullptr;

    const D3D12_DESCRIPTOR_RANGE descRanges[]{
        // u0 (buffer-filling compute shader)
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = 1U,
            .BaseShaderRegister = 0U,
            .RegisterSpace = 0U,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        },
        // u1 (buffer-filling compute shader)
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = 1U,
            .BaseShaderRegister = 1U,
            .RegisterSpace = 0U,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        },
        // b0 (draw/draw index command rotate constant buffer)
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
            .NumDescriptors = 1U,
            .BaseShaderRegister = 0U,
            .RegisterSpace = 0U,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        }
    };

    const D3D12_ROOT_PARAMETER rootParameters[]{
        {
            // unordered access view (UAV) for u0 (buffer-filling compute shader)
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[0]
            },
            // This unordered access buffer will be accessed in a compute shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        {
            // unordered access view (UAV) for u1 (buffer-filling compute shader)
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[1]
            },
            // This unordered access buffer will be accessed in a compute shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        {
            // b0 (draw/draw index command rotate constant buffer)
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[2]
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
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
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

// @return [pipelineState, commandList, commandBundleList, descriptorHeap]
static auto CreatePipelineStateObjectForArgBufferCompute(ID3D12Device* d3d_device, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator,
                                                        ID3D12RootSignature* rootSignature) -> std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;
    ID3D12DescriptorHeap* descriptorHeap = nullptr;

    auto result = std::make_tuple(pipelineState, commandList, commandBundle, descriptorHeap);

    D3D12_SHADER_BYTECODE computeShaderObj = CreateCompiledShaderObjectFromPath("shaders/exec_indirect.comp.cso");

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

        const D3D12_DESCRIPTOR_HEAP_DESC cbv_srv_uavHeapDesc {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = CBV_SRV_UAV_SLOT_ID::CBV_SRV_UAV_SLOT_COUNT,      // This descriptor heap is for all of CBV, UAV and SRV buffers.
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0
        };
        hRes = d3d_device->CreateDescriptorHeap(&cbv_srv_uavHeapDesc, IID_PPV_ARGS(&descriptorHeap));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateDescriptorHeap for unordered access view failed: %ld\n", hRes);
            return result;
        }

        result = std::make_tuple(pipelineState, commandList, commandBundle, descriptorHeap);
    }
    while (false);

    if (computeShaderObj.pShaderBytecode != nullptr) {
        free((void*)computeShaderObj.pShaderBytecode);
    }

    return result;
}

// @return [pipelineState, commandList, commandBundle]
static auto CreatePipelineStateObjectDraw(ID3D12Device* d3d_device, ID3D12CommandAllocator *commandAllocator, ID3D12CommandAllocator* commandBundleAllocator, ID3D12RootSignature* rootSignature) ->
                                        std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;

    auto result = std::make_tuple(pipelineState, commandList, commandBundle);

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("shaders/exec_indirect_draw.vert.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("shaders/exec_indirect.frag.cso");

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

        hRes = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, commandBundleAllocator, pipelineState, IID_PPV_ARGS(&commandBundle));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommandList for command bundle failed: %ld\n", hRes);
            break;
        }

        result = std::make_tuple(pipelineState, commandList, commandBundle);
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

// @return [pipelineState, commandBundle]
static auto CreatePipelineStateObjectDrawIndexed(ID3D12Device* d3d_device, ID3D12CommandAllocator* commandBundleAllocator, ID3D12RootSignature* rootSignature) ->
                                                std::pair<ID3D12PipelineState*, ID3D12GraphicsCommandList*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;

    auto result = std::make_pair(pipelineState, commandBundle);

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("shaders/exec_indirect_draw.vert.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("shaders/exec_indirect.frag.cso");

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
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
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

        hRes = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, commandBundleAllocator, pipelineState, IID_PPV_ARGS(&commandBundle));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommandList for command bundle failed: %ld\n", hRes);
            break;
        }

        result = std::make_pair(pipelineState, commandBundle);
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

// @return [pipelineState, commandBundle]
static auto CreatePipelineStateObjectForMeshShader(ID3D12Device2* d3d_device, ID3D12RootSignature* rootSignature,
                                                ID3D12CommandAllocator* commandBundleAllocator) ->
                                                std::pair<ID3D12PipelineState*, ID3D12GraphicsCommandList*>
{
    D3D12_SHADER_BYTECODE amplificationShaderObj = CreateCompiledShaderObjectFromPath("shaders/ms.amplification.cso");
    D3D12_SHADER_BYTECODE meshShaderObj = CreateCompiledShaderObjectFromPath("shaders/ms.mesh.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("shaders/basic.frag.cso");

    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;

    std::pair<ID3D12PipelineState*, ID3D12GraphicsCommandList*> result = std::make_pair(pipelineState, commandBundle);

    bool done = false;
    do
    {
        if (amplificationShaderObj.pShaderBytecode == nullptr || amplificationShaderObj.BytecodeLength == 0) break;

        if (meshShaderObj.pShaderBytecode == nullptr || meshShaderObj.BytecodeLength == 0) break;

        if (pixelShaderObj.pShaderBytecode == nullptr || pixelShaderObj.BytecodeLength == 0) break;

        // Describe and create the graphics pipeline state object (PSO).
        // The format of the provided stream should consist of an alternating set of D3D12_PIPELINE_STATE_SUBOBJECT_TYPE, 
        // and the corresponding subobject types for them.
        // For example, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER pairs with D3D12_RASTERIZER_DESC.
        struct {
            RootSignatureSubobject rootSignatureSubobject;
            ShaderByteCodeSubobject asShaderSubobject;
            ShaderByteCodeSubobject msShaderSubobject;
            ShaderByteCodeSubobject psShaderSubobject;
            BlendStateSubobject blendStateSubobject;
            SampleMaskSubobject sampleMaskSubobject;
            RasterizerStateSubobject raterizerStateSubobject;
            DepthStencilSubobject depthStencilSubobject;
            IBStripCutValueSubobject ibStripCutValueSubobject;
            PrimitiveTopologyTypeSubobject primitiveTopologySubobject;
            RenderTargetFormatsSubobject renderTargetFormatsSubobject;
            DepthStencilViewFormat depthStencilViewFormatSubobject;
            SampleDescSubobject sampleDescSubobject;
            NodeMaskSubobject nodeMaskSubobject;
            CachedPSOSubobject cachedPSOSubobject;
            FlagsSubobject flagsSubobject;
        } psoStream {
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, rootSignature },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, amplificationShaderObj },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, meshShaderObj },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, pixelShaderObj },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND, {
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
                }
            },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK, UINT32_MAX },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, {
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
                }
            },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL, {
                    .DepthEnable = FALSE,
                    .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
                    .DepthFunc = D3D12_COMPARISON_FUNC_NEVER,
                    .StencilEnable = FALSE,
                    .StencilReadMask = 0,
                    .StencilWriteMask = 0,
                    .FrontFace {},
                    .BackFace { }
                }
            },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, {
                    .RTFormats {
                        // RTVFormats[0]
                        { RENDER_TARGET_BUFFER_FOMRAT }
                    },
                    .NumRenderTargets = 1
                }
            },
            { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT, DXGI_FORMAT_UNKNOWN },
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

        hRes = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, commandBundleAllocator, pipelineState, IID_PPV_ARGS(&commandBundle));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommandList for command bundle failed: %ld\n", hRes);
            break;
        }

        done = true;
    }
    while (false);

    if (amplificationShaderObj.pShaderBytecode != nullptr) {
        free((void*)amplificationShaderObj.pShaderBytecode);
    }
    if (meshShaderObj.pShaderBytecode != nullptr) {
        free((void*)meshShaderObj.pShaderBytecode);
    }
    if (pixelShaderObj.pShaderBytecode != nullptr) {
        free((void*)pixelShaderObj.pShaderBytecode);
    }

    result = std::make_pair(pipelineState, commandBundle);
    return result;
}

// @return [indirectArgumentBuffer, indirectCountBuffer]
static auto CreateComputeBuffersForArgumentFilling(ID3D12Device* d3d_device, ID3D12RootSignature* computeRootSignature, ID3D12CommandQueue* commandQueue,
                                                ID3D12GraphicsCommandList* commandList, ID3D12GraphicsCommandList* commandBundle, ID3D12DescriptorHeap* descriptorHeap) ->
                                                std::pair< ID3D12Resource*, ID3D12Resource*>
{
    ID3D12Resource* indirectArgumentBuffer = nullptr;
    ID3D12Resource* indirectCountBuffer = nullptr;

    auto result = std::make_pair(indirectArgumentBuffer, indirectCountBuffer);

    // ======== Create indirectArgumentBuffer and indirectCountBuffer ========

    const D3D12_HEAP_PROPERTIES defaultHeapProperties{
        .Type = D3D12_HEAP_TYPE_DEFAULT,    // default heap type for device visible memory
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1
    };

    constexpr UINT indirectArgumentBuffer_elemCount = 6U;
    constexpr UINT indirectCountBuffer_elemCount = 3U;
    constexpr UINT indirectArgumentBuffer_elemSize = (UINT)sizeof(IndirectArgumentBufferType);
    constexpr UINT indirectCountBuffer_elemSize = (UINT)sizeof(UINT);

    D3D12_RESOURCE_DESC uavResourceDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = indirectArgumentBuffer_elemCount * indirectArgumentBuffer_elemSize,
        .Height = 1U,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc {.Count = 1U, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
    };

    HRESULT hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavResourceDesc,
                                                D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&indirectArgumentBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for indirectArgumentBuffer failed: %ld\n", hRes);
        return result;
    }

    result = std::make_pair(indirectArgumentBuffer, indirectCountBuffer);

    uavResourceDesc.Width = indirectCountBuffer_elemCount * indirectCountBuffer_elemSize;
    hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavResourceDesc,
                                                D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&indirectCountBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for indirectCountBuffer failed: %ld\n", hRes);
        return result;
    }

    result = std::make_pair(indirectArgumentBuffer, indirectCountBuffer);

    // ======== Create Unordered Access Buffer Views ========

    const UINT descriptorIncrSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE indirectArgumentBufferCPUDescHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE indirectCountBufferCPUDescHandle = indirectArgumentBufferCPUDescHandle;
    indirectArgumentBufferCPUDescHandle.ptr += INDIRECT_ARGUMENT_BUFFER_UAV_SLOT * descriptorIncrSize;
    indirectCountBufferCPUDescHandle.ptr += INDIRECT_COUNT_BUFFER_UAV_SLOT * descriptorIncrSize;

    // Create the unordered access buffer view for indirectArgumentBuffer
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer {
            .FirstElement = 0,
            .NumElements = indirectArgumentBuffer_elemCount,
            .StructureByteStride = indirectArgumentBuffer_elemSize,
            .CounterOffsetInBytes = 0,
            .Flags = D3D12_BUFFER_UAV_FLAG_NONE
        }
    };
    d3d_device->CreateUnorderedAccessView(indirectArgumentBuffer, nullptr, &uavDesc, indirectArgumentBufferCPUDescHandle);

    // Create the unordered access buffer view for indirectCountBuffer
    uavDesc.Buffer.NumElements = indirectCountBuffer_elemCount;
    uavDesc.Buffer.StructureByteStride = indirectCountBuffer_elemSize;
    d3d_device->CreateUnorderedAccessView(indirectCountBuffer, nullptr, &uavDesc, indirectCountBufferCPUDescHandle);


    D3D12_GPU_DESCRIPTOR_HANDLE indirectArgumentBufferGPUDescHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE indirectCountBufferGPUDescHandle = indirectArgumentBufferGPUDescHandle;
    indirectArgumentBufferGPUDescHandle.ptr += INDIRECT_ARGUMENT_BUFFER_UAV_SLOT * descriptorIncrSize;
    indirectCountBufferGPUDescHandle.ptr += INDIRECT_COUNT_BUFFER_UAV_SLOT * descriptorIncrSize;

    // ======== Populate Execution Commands ========

    ID3D12DescriptorHeap* const descHeaps[]{ descriptorHeap };
    commandList->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);

    commandBundle->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
    commandBundle->SetComputeRootSignature(computeRootSignature);
    commandBundle->SetComputeRootDescriptorTable(0, indirectArgumentBufferGPUDescHandle);
    commandBundle->SetComputeRootDescriptorTable(1, indirectCountBufferGPUDescHandle);
    commandBundle->Dispatch(1U, 1U, 1U);

    hRes = commandBundle->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close compute command bundle failed: %ld\n", hRes);
        return result;
    }

    commandList->ExecuteBundle(commandBundle);

    const D3D12_RESOURCE_BARRIER uavBarriers[] = {
        {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition {
                .pResource = indirectArgumentBuffer,
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                .StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
            }
        },
        {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition {
                .pResource = indirectCountBuffer,
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                .StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
            }
        }
    };
    commandList->ResourceBarrier(UINT(std::size(uavBarriers)), uavBarriers);

    hRes = commandList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close compute command bundle failed: %ld\n", hRes);
        return result;
    }

    ID3D12CommandList* const computeCommandLists[] = { (ID3D12CommandList*)commandList };
    commandQueue->ExecuteCommandLists((UINT)std::size(computeCommandLists), computeCommandLists);

    // Wait for all the above commands completing the execution
    WaitForPreviousFrame(commandQueue);

    return result;
}

// @return std::make_tuple(commandSignature, uploadDevHostBuffer, vertexBuffer, rotateConstantBuffer, vertexBufferView)
static auto CreateVertexBuffer(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12GraphicsCommandList* commandList,
                            ID3D12GraphicsCommandList* commandBundle, ID3D12DescriptorHeap *descriptorHeap) ->
                            std::tuple<ID3D12CommandSignature*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, D3D12_VERTEX_BUFFER_VIEW>
{
    struct Vertex
    {
        float position[4];
        float color[4];
    } squareVertices[16 + 361]{
        // Direct3D是以左手作为前面背面顶点排列的依据

        // Left triangle
        {.position { -0.25f, 0.25f, 0.0f, 0.0f }, .color { 0.9f, 0.1f, 0.1f, 1.0f } },     // top left
        {.position { 0.25f, 0.25f, 0.0f, 0.0f }, .color { 0.9f, 0.9f, 0.1f, 1.0f } },      // top right
        {.position { -0.25f, -0.25f, 0.0f, 0.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },    // bottom left
        {.position { 0.25f, -0.25f, 0.0f, 0.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } },     // bottom right

        // Top triangle
        {.position { -0.25f, 0.25f, 0.0f, 1.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },     // top left
        {.position { 0.25f, 0.25f, 0.0f, 1.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } },      // top right
        {.position { -0.25f, -0.25f, 0.0f, 1.0f }, .color { 0.9f, 0.9f, 0.1f, 1.0f } },    // bottom left
        {.position { 0.25f, -0.25f, 0.0f, 1.0f }, .color { 0.9f, 0.1f, 0.1f, 1.0f } },     // bottom right

        // Right triangle
        {.position { -0.25f, 0.25f, 0.0f, 2.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } },     // top left
        {.position { 0.25f, 0.25f, 0.0f, 2.0f }, .color { 0.9f, 0.1f, 0.1f, 1.0f } },      // top right
        {.position { -0.25f, -0.25f, 0.0f, 2.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },    // bottom left
        {.position { 0.25f, -0.25f, 0.0f, 2.0f }, .color { 0.9f, 0.9f, 0.1f, 1.0f } },     // bottom right

        // Bottom triangle
        {.position { -0.25f, 0.25f, 0.0f, 3.0f }, .color { 0.9f, 0.9f, 0.1f, 1.0f } },     // top left
        {.position { 0.25f, 0.25f, 0.0f, 3.0f }, .color { 0.9f, 0.1f, 0.1f, 1.0f } },      // top right
        {.position { -0.25f, -0.25f, 0.0f, 3.0f }, .color { 0.1f, 0.9f, 0.1f, 1.0f } },    // bottom left
        {.position { 0.25f, -0.25f, 0.0f, 3.0f }, .color { 0.1f, 0.1f, 0.9f, 1.0f } }      // bottom right
    };

    // Fill indexed line primitives
    squareVertices[16] = Vertex{ .position { 0.0f, 0.0f, 0.0f, 4.0f }, .color { 0.9f, 0.9f, 0.1f, 1.0f } };
    for (int i = 17; i < 17 + 360; ++i)
    {
        const float index = float(i - 17);
        const float x = 0.25f * (float)cos(index * M_PI / 180.0);
        const float y = 0.25f * (float)sin(index * M_PI / 180.0);
        squareVertices[i] = Vertex{ .position { x, y, 0.0f, 4.0f }, .color { 0.9f, 0.1f + 0.8f / 359.0f * index, 0.1f, 1.0f} };
    }

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

    ID3D12CommandSignature* commandSignature = nullptr;
    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* rotateConstantBuffer = nullptr;

    auto result = std::make_tuple(commandSignature, uploadDevHostBuffer, vertexBuffer, rotateConstantBuffer, D3D12_VERTEX_BUFFER_VIEW{});

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
    uploadResourceDesc.Width += (2U * 360U) * sizeof(UINT);
    hRes = d3d_device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadResourceDesc,
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

    // Copy vertex data
    memcpy(hostMemPtr, squareVertices, sizeof(squareVertices));

    // Assign index data
    unsigned* indexBufferPtr = (unsigned*)(uintptr_t(hostMemPtr) + sizeof(squareVertices));
    for (unsigned i = 0U; i < 360U; ++i)
    {
        indexBufferPtr[i * 2 + 0] = 16U;
        indexBufferPtr[i * 2 + 1] = 17U + i;
    }

    uploadDevHostBuffer->Unmap(0, nullptr);

    // Upload vertex data
    WriteToDeviceResourceAndSync(commandList, vertexBuffer, uploadDevHostBuffer, 0U, 0U, sizeof(squareVertices));

    // Initialize the vertex buffer view.
    const D3D12_VERTEX_BUFFER_VIEW vertexBufferView{
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

    // Create rotate constant buffer object
    hRes = d3d_device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &cbResourceDesc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&rotateConstantBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for rotate constant buffer failed: %ld\n", hRes);
        return result;
    }

    // Upload data to constant buffer
    hRes = rotateConstantBuffer->Map(0, nullptr, &hostMemPtr);
    if (FAILED(hRes))
    {
        fprintf(stderr, "Map constant buffer failed: %ld\n", hRes);
        return result;
    }

    memset(hostMemPtr, 0, CONSTANT_BUFFER_ALLOCATION_GRANULARITY);

    rotateConstantBuffer->Unmap(0, nullptr);

    // Fetch CBV and UAV CPU descriptor handles
    auto const descHandleIncrSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE indirectArgumentBufferCPUDescHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE rotateCBVCPUDescHandle = indirectArgumentBufferCPUDescHandle;
    rotateCBVCPUDescHandle.ptr += DRAW_COMMAND_ROTATE_CBV_SLOT * descHandleIncrSize;

    // Create the constant buffer view
    const D3D12_CONSTANT_BUFFER_VIEW_DESC rotateCBVDesc{
        .BufferLocation = rotateConstantBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = CONSTANT_BUFFER_ALLOCATION_GRANULARITY
    };
    d3d_device->CreateConstantBufferView(&rotateCBVDesc, rotateCBVCPUDescHandle);

    // Fetch CBV and UAV GPU descriptor handles
    D3D12_GPU_DESCRIPTOR_HANDLE indirectArgumentBufferGPUDescHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE rotateCBVGPUDescHandle = indirectArgumentBufferGPUDescHandle;
    rotateCBVGPUDescHandle.ptr += DRAW_COMMAND_ROTATE_CBV_SLOT * descHandleIncrSize;

    // Create Command Signature
    const D3D12_INDIRECT_ARGUMENT_DESC argumentDescList[]{
        {
            .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW
        }
    };

    const D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc{
        // sizeof(IndirectArgumentBufferType) MUST BE at least 32 bytes.
        .ByteStride = (UINT)sizeof(IndirectArgumentBufferType),
        .NumArgumentDescs = (UINT)std::size(argumentDescList),
        .pArgumentDescs = argumentDescList,
        .NodeMask = 0U
    };

    hRes = d3d_device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&commandSignature));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommandSignature for draw commands failed: %ld\n", hRes);
        return result;
    }

    // Record commands to the command list bundle.
    commandBundle->SetGraphicsRootSignature(rootSignature);
    ID3D12DescriptorHeap* const descHeaps[]{ descriptorHeap };
    // ATTENTION: SetDescriptorHeaps should be set into command bundle list as well as command list
    commandBundle->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
    commandBundle->SetGraphicsRootDescriptorTable(2, rotateCBVGPUDescHandle);  // rootParameters[2]
    commandBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    commandBundle->IASetVertexBuffers(0, 1, &vertexBufferView);

    // ATTENTION: ExecuteIndirect can only be called on a bundle when the count buffer is set to NULL, 
    // and the command signature contains exactly 1 parameter (which must be a Draw, DrawIndexed, Dispatch or DispatchRays)
    // commandBundle->ExecuteIndirect(commandSignature, 4U, indirectArgumentBuffer, 0U, nullptr, 0U);

    // End of the record
    hRes = commandBundle->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close basic command bundle failed: %ld\n", hRes);
        return result;
    }

    result = std::make_tuple(commandSignature, uploadDevHostBuffer, vertexBuffer, rotateConstantBuffer, vertexBufferView);
    return result;
}

// @return [commandSignature, indexBuffer]
static auto CreateVertexBufferIndexed(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12GraphicsCommandList *commandList, ID3D12GraphicsCommandList* commandBundle,
                                    ID3D12DescriptorHeap* descriptorHeap, const D3D12_VERTEX_BUFFER_VIEW &vertexBufferView,
                                    ID3D12Resource* uploadDevHostBuffer) ->
                                    std::pair<ID3D12CommandSignature*, ID3D12Resource*>
{
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

    constexpr size_t indexBufferSize = (2U * 360U) * sizeof(UINT);

    const D3D12_RESOURCE_DESC indexResourceDesc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = indexBufferSize,
        .Height = 1U,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc {.Count = 1U, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    ID3D12CommandSignature* commandSignature = nullptr;
    ID3D12Resource* indexBuffer = nullptr;

    auto result = std::make_pair(commandSignature, indexBuffer);

    // Create indexBuffer on GPU side.
    HRESULT hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &indexResourceDesc,
                                                    D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&indexBuffer));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommittedResource for vertex buffer failed: %ld\n", hRes);
        return result;
    }

    // upload index data
    WriteToDeviceResourceAndSync(commandList, indexBuffer, uploadDevHostBuffer, 0U, vertexBufferView.SizeInBytes, indexBufferSize);

    auto const descHandleIncrSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Create index buffer view
    const D3D12_INDEX_BUFFER_VIEW indexBufferView{
        .BufferLocation = indexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = indexBufferSize,
        .Format = DXGI_FORMAT_R32_UINT
    };

    // Fetch CBV and UAV GPU descriptor handles
    D3D12_GPU_DESCRIPTOR_HANDLE indirectArgumentBufferGPUDescHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE rotateCBVGPUDescHandle = indirectArgumentBufferGPUDescHandle;
    rotateCBVGPUDescHandle.ptr += DRAW_COMMAND_ROTATE_CBV_SLOT * descHandleIncrSize;

    // Create Command Signature
    const D3D12_INDIRECT_ARGUMENT_DESC argumentDescList[]{
        {
            .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED
        }
    };

    const D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc{
        // sizeof(IndirectArgumentBufferType) MUST BE at least 32 bytes.
        .ByteStride = (UINT)sizeof(IndirectArgumentBufferType),
        .NumArgumentDescs = (UINT)std::size(argumentDescList),
        .pArgumentDescs = argumentDescList,
        .NodeMask = 0U
    };

    hRes = d3d_device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&commandSignature));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommandSignature for draw commands failed: %ld\n", hRes);
        return result;
    }

    // Record commands to the command list bundle.
    commandBundle->SetGraphicsRootSignature(rootSignature);
    ID3D12DescriptorHeap* const descHeaps[]{ descriptorHeap };
    // ATTENTION: SetDescriptorHeaps should be set into command bundle list as well as command list
    commandBundle->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
    commandBundle->SetGraphicsRootDescriptorTable(2, rotateCBVGPUDescHandle);  // rootParameters[2]
    commandBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    commandBundle->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandBundle->IASetIndexBuffer(&indexBufferView);

    // End of the record
    hRes = commandBundle->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close basic command bundle failed: %ld\n", hRes);
        return result;
    }

    result = std::make_pair(commandSignature, indexBuffer);
    return result;
}

static auto CreateCommandSignatureForMeshShader(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12GraphicsCommandList* commandBundle) -> ID3D12CommandSignature*
{
    // Create Command Signature
    const D3D12_INDIRECT_ARGUMENT_DESC argumentDescList[]{
        {
            .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH
        }
    };

    const D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc{
        // sizeof(IndirectArgumentBufferType) MUST BE at least 32 bytes.
        .ByteStride = (UINT)sizeof(IndirectArgumentBufferType),
        .NumArgumentDescs = (UINT)std::size(argumentDescList),
        .pArgumentDescs = argumentDescList,
        .NodeMask = 0U
    };

    ID3D12CommandSignature* commandSignature = nullptr;

    HRESULT hRes = d3d_device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&commandSignature));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateCommandSignature for draw commands failed: %ld\n", hRes);
        return commandSignature;
    }

    // Record commands to the command list bundle.
    commandBundle->SetGraphicsRootSignature(rootSignature);

    // End of the record
    hRes = commandBundle->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close basic command bundle failed: %ld\n", hRes);
        return commandSignature;
    }

    return commandSignature;
}

static auto ExecuteDataCopyCommandsAndWait(ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList* commandList) -> bool
{
    HRESULT hRes = commandList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close command list failed: %ld\n", hRes);
        return false;
    }

    // Execute the command list to complete the copy operation
    ID3D12CommandList* const ppCommandLists[] = { (ID3D12CommandList*)commandList };
    commandQueue->ExecuteCommandLists((UINT)std::size(ppCommandLists), ppCommandLists);

    // Wait for the command list to execute;
    // we are reusing the same command list in our main loop but for now,
    // we just want to wait for setup to complete before continuing.
    WaitForPreviousFrame(commandQueue);

    return true;
}

auto CreateExecuteIndirectTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue *commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator, bool supportMeshShader) ->
                                    std::tuple<ID3D12RootSignature*, std::array<ID3D12PipelineState*, 3>, ID3D12GraphicsCommandList*, std::array<ID3D12GraphicsCommandList*, 3>, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, std::array<ID3D12CommandSignature*, 3>, bool>
{
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* computePipelineStateForArgumentBufferFilling = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12PipelineState* pipelineStateIndexed = nullptr;
    ID3D12PipelineState* pipelineStateMeshShader = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;
    ID3D12GraphicsCommandList* commandBundleIndexed = nullptr;
    ID3D12GraphicsCommandList* commandBundleMeshShader = nullptr;
    ID3D12DescriptorHeap* descriptorHeap = nullptr;
    ID3D12CommandSignature* drawCommandsSignature = nullptr;
    ID3D12CommandSignature* drawIndexedCommandSignature = nullptr;
    ID3D12CommandSignature* meshShaderCommandSignature = nullptr;
    ID3D12Resource* indirectArgumentBuffer = nullptr;
    ID3D12Resource* indirectCountBuffer = nullptr;
    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* indexBuffer = nullptr;
    ID3D12Resource* rotateConstantBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{ };

    bool success = false;

    auto const result = std::make_tuple(rootSignature, pipelineState, commandList, std::array<ID3D12GraphicsCommandList*, 3>(), descriptorHeap, uploadDevHostBuffer, vertexBuffer, indexBuffer, rotateConstantBuffer, indirectArgumentBuffer, indirectCountBuffer, std::array<ID3D12CommandSignature*, 3>(), success);

    success = true;

    rootSignature = CreateRootSignature(d3d_device);
    if (rootSignature == nullptr) {
        success = false;
    }

    auto const computePipelineResult = CreatePipelineStateObjectForArgBufferCompute(d3d_device, commandAllocator, commandBundleAllocator, rootSignature);
    computePipelineStateForArgumentBufferFilling = std::get<0>(computePipelineResult);
    commandList = std::get<1>(computePipelineResult);
    commandBundle = std::get<2>(computePipelineResult);
    descriptorHeap = std::get<3>(computePipelineResult);
    if (computePipelineStateForArgumentBufferFilling == nullptr || descriptorHeap == nullptr) {
        success = false;
    }

    auto const computeBuffersResult = CreateComputeBuffersForArgumentFilling(d3d_device, rootSignature, commandQueue, commandList, commandBundle, descriptorHeap);
    indirectArgumentBuffer = computeBuffersResult.first;
    indirectCountBuffer = computeBuffersResult.second;
    if (commandList == nullptr || commandBundle == nullptr) {
        success = false;
    }

    if (computePipelineStateForArgumentBufferFilling != nullptr) {
        computePipelineStateForArgumentBufferFilling->Release();
    }
    if (commandList != nullptr) {
        commandList->Release();
    }
    if (commandBundle != nullptr) {
        commandBundle->Release();
    }

    auto const graphicsPipelineResult = CreatePipelineStateObjectDraw(d3d_device, commandAllocator, commandBundleAllocator, rootSignature);
    pipelineState = std::get<0>(graphicsPipelineResult);
    commandList = std::get<1>(graphicsPipelineResult);
    commandBundle = std::get<2>(graphicsPipelineResult);
    if (commandList == nullptr || commandBundle == nullptr) {
        success = false;
    }

    auto const vertexBufferResult = CreateVertexBuffer(d3d_device, rootSignature, commandList, commandBundle, descriptorHeap);
    drawCommandsSignature = std::get<0>(vertexBufferResult);
    uploadDevHostBuffer = std::get<1>(vertexBufferResult);
    vertexBuffer = std::get<2>(vertexBufferResult);
    rotateConstantBuffer = std::get<3>(vertexBufferResult);
    vertexBufferView = std::get<4>(vertexBufferResult);
    if (uploadDevHostBuffer == nullptr || vertexBuffer == nullptr || rotateConstantBuffer == nullptr) {
        success = false;
    }

    auto const indexedPipelineState = CreatePipelineStateObjectDrawIndexed(d3d_device, commandBundleAllocator, rootSignature);
    pipelineStateIndexed = indexedPipelineState.first;
    commandBundleIndexed = indexedPipelineState.second;
    if (pipelineStateIndexed == nullptr || commandBundleIndexed == nullptr) {
        success = false;
    }

    auto const indexedVertexBufferResult = CreateVertexBufferIndexed(d3d_device, rootSignature, commandList, commandBundleIndexed, descriptorHeap, vertexBufferView, uploadDevHostBuffer);
    drawIndexedCommandSignature = indexedVertexBufferResult.first;
    indexBuffer = indexedVertexBufferResult.second;

    if (supportMeshShader)
    {
        auto const meshShaderPipelineState = CreatePipelineStateObjectForMeshShader((ID3D12Device2*)d3d_device, rootSignature, commandBundleAllocator);
        pipelineStateMeshShader = meshShaderPipelineState.first;
        commandBundleMeshShader = meshShaderPipelineState.second;
        if (pipelineStateMeshShader == nullptr || commandBundleMeshShader == nullptr) {
            success = false;
        }

        meshShaderCommandSignature = CreateCommandSignatureForMeshShader(d3d_device, rootSignature, commandBundleMeshShader);
        if (meshShaderCommandSignature == nullptr) {
            success = false;
        }
    }

    if (!ExecuteDataCopyCommandsAndWait(commandQueue, commandList)) {
        success = false;
    }

    std::array< ID3D12PipelineState*, 3> pipelineStateArray{ pipelineState, pipelineStateIndexed, pipelineStateMeshShader };
    std::array<ID3D12GraphicsCommandList*, 3> commandBundleArray { commandBundle, commandBundleIndexed, commandBundleMeshShader };
    std::array<ID3D12CommandSignature*, 3> commandSignatureArray{ drawCommandsSignature, drawIndexedCommandSignature, meshShaderCommandSignature };

    return std::make_tuple(rootSignature, pipelineStateArray, commandList, commandBundleArray, descriptorHeap, uploadDevHostBuffer, vertexBuffer, indexBuffer, rotateConstantBuffer, indirectArgumentBuffer, indirectCountBuffer, commandSignatureArray, success);
}

auto ExecuteIndirectCallbackHandler(ID3D12GraphicsCommandList* commandList, ID3D12CommandSignature* commandSignature, ID3D12Resource* indirectArgumentBuffer, ID3D12Resource* indirectCountBuffer, UINT index) -> void
{
    constexpr UINT64 secondCommandsOffset = UINT64(4U * sizeof(IndirectArgumentBufferType));
    constexpr UINT64 thirdCommandsOffset = UINT64(secondCommandsOffset + 1U * sizeof(IndirectArgumentBufferType));

    UINT maxCommandCount = 1U;
    UINT64 argumentBufferOffset = 0U, countBufferOffset = 0U;

    switch (index)
    {
    case 0U:
    default:
        maxCommandCount = 4U;
        break;

    case 1U:
        argumentBufferOffset = secondCommandsOffset;
        countBufferOffset = 4U;
        break;

    case 2U:
        argumentBufferOffset = thirdCommandsOffset;
        countBufferOffset = 8U;
        break;
    }

    commandList->ExecuteIndirect(commandSignature, maxCommandCount, indirectArgumentBuffer, argumentBufferOffset, indirectCountBuffer, countBufferOffset);
}

