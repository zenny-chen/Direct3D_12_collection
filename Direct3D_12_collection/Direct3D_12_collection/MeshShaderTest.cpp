#include "common.h"

static auto CreateMeshShaderBasicRootSignature(MeshShaderExecMode execMode, ID3D12Device *d3d_device) -> ID3D12RootSignature*
{
    ID3D12RootSignature* rootSignature = nullptr;

    const D3D12_ROOT_PARAMETER rootParameter{
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
        .Constants {
            .ShaderRegister = 0,
            .RegisterSpace = 0,
            .Num32BitValues = 16    // store 4 float4 colors
        },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_MESH
    };

    const UINT nParameters = execMode == MeshShaderExecMode::ONLY_MESH_SHADER_MODE ? 1U : 0U;
    const D3D12_ROOT_PARAMETER* pRootParameter = execMode == MeshShaderExecMode::ONLY_MESH_SHADER_MODE ? &rootParameter : nullptr;
    D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                                        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    if (execMode == MeshShaderExecMode::ONLY_MESH_SHADER_MODE) {
        flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;
    }

    // Create a root signature.
    const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {
        .NumParameters = nParameters,
        .pParameters = pRootParameter,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = flags
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

static auto CreateMeshShaderBasicPipelineStateObject(MeshShaderExecMode execMode, ID3D12Device2* d3d_device, ID3D12RootSignature* rootSignature,
                                                    ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
            std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*>
{
    D3D12_SHADER_BYTECODE amplificationShaderObj{ };
    D3D12_SHADER_BYTECODE meshShaderObj{ };
    D3D12_SHADER_BYTECODE pixelShaderObj{ };

    switch (execMode)
    {
    case MeshShaderExecMode::BASIC_MODE:
        amplificationShaderObj = CreateCompiledShaderObjectFromPath("shaders/ms.amplification.cso");
        meshShaderObj = meshShaderObj = CreateCompiledShaderObjectFromPath("shaders/ms.mesh.cso");
        pixelShaderObj = CreateCompiledShaderObjectFromPath("shaders/basic.frag.cso");
        break;

    case MeshShaderExecMode::ONLY_MESH_SHADER_MODE:
        meshShaderObj = meshShaderObj = CreateCompiledShaderObjectFromPath("shaders/msonly.mesh.cso");
        pixelShaderObj = CreateCompiledShaderObjectFromPath("shaders/basic.frag.cso");
        break;
    }

    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;

    std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*> result = std::make_tuple(pipelineState, commandList, commandBundle);

    bool done = false;
    do
    {
        if (execMode == MeshShaderExecMode::BASIC_MODE) {
            if (amplificationShaderObj.pShaderBytecode == nullptr || amplificationShaderObj.BytecodeLength == 0) break;
        }

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
                        .MultisampleEnable = FALSE,
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
                        { DXGI_FORMAT_R8G8B8A8_UNORM }
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

    if (amplificationShaderObj.pShaderBytecode != nullptr) {
        free((void*)amplificationShaderObj.pShaderBytecode);
    }
    if (meshShaderObj.pShaderBytecode != nullptr) {
        free((void*)meshShaderObj.pShaderBytecode);
    }
    if (pixelShaderObj.pShaderBytecode != nullptr) {
        free((void*)pixelShaderObj.pShaderBytecode);
    }

    result = std::make_tuple(pipelineState, commandList, commandBundle);
    return result;
}

static auto PopulateMeshShaderCommandBundleList(MeshShaderExecMode execMode, ID3D12RootSignature *rootSignature,
                                            ID3D12GraphicsCommandList *commandList, ID3D12GraphicsCommandList6 *commandBundleList) -> bool
{
    // No subsequent commands need be recorded into this command list,
    // so close it now.
    HRESULT hRes = commandList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close Mesh Shader command list failed: %ld\n", hRes);
        return false;
    }

    // Record commands to the command list bundle.
    commandBundleList->SetGraphicsRootSignature(rootSignature);

    if (execMode == MeshShaderExecMode::ONLY_MESH_SHADER_MODE)
    {
        const struct
        {
            float bottomLeft[4];
            float topLeft[4];
            float bottomRight[4];
            float topRight[4];
        } colorVaryings = {
            { 0.9f, 0.1f, 0.1f, 1.0f },     // red
            { 0.1f, 0.9f, 0.1f, 1.0f },     // green
            { 0.1f, 0.1f, 0.9f, 1.0f },     // blue
            { 0.9f, 0.9f, 0.1f, 1.0f }      // yellow
        };

        commandBundleList->SetGraphicsRoot32BitConstants(0, 16, &colorVaryings, 0);
        commandBundleList->DispatchMesh(4U, 1U, 1U);
    }
    else {
        commandBundleList->DispatchMesh(1U, 1U, 1U);
    }
    
    // End of the record
    hRes = commandBundleList->Close();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Close Mesh Shader command bundle failed: %ld\n", hRes);
        return false;
    }

    return true;
}

auto CreateMeshShaderTestAssets(MeshShaderExecMode execMode, ID3D12Device* d3d_device, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*>
{
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;

    std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*> result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundleList);

    rootSignature = CreateMeshShaderBasicRootSignature(execMode, d3d_device);
    if (rootSignature == nullptr) return result;

    auto pipelineRes = CreateMeshShaderBasicPipelineStateObject(execMode, (ID3D12Device2*)d3d_device, rootSignature, commandAllocator, commandBundleAllocator);
    pipelineState = std::get<0>(pipelineRes);
    commandList = std::get<1>(pipelineRes);
    commandBundleList = std::get<2>(pipelineRes);

    if (!PopulateMeshShaderCommandBundleList(execMode, rootSignature, commandList, (ID3D12GraphicsCommandList6*)commandBundleList)) return result;

    result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundleList);
    return result;
}

