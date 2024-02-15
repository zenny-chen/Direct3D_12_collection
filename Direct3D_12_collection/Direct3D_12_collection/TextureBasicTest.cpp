#include "common.h"


static auto CreateRootSignature(ID3D12Device* d3d_device) -> ID3D12RootSignature*
{
    ID3D12RootSignature* rootSignature = nullptr;

    const D3D12_DESCRIPTOR_RANGE descRanges[]{
        // t0
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        },
        // s0
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        },
        // b0
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        }
    };

    const D3D12_ROOT_PARAMETER rootParameters[] {
        {
            // shader resource view (SRV) for s0
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[0]
            },
            // This texture buffer will just be accessed in a pixel shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
        },
        {
            // sampler for s0
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &descRanges[1]
            },
            // This constant buffer will just be accessed in a vertex shader
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
        },
        {
            // constant buffer view (CBV) for b0
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

static auto CreatePipelineStateObject(ID3D12Device* d3d_device, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator, ID3D12RootSignature* rootSignature) ->
                                                    std::tuple<ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*>
{
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundleList = nullptr;

    D3D12_SHADER_BYTECODE vertexShaderObj = CreateCompiledShaderObjectFromPath("cso/basic_texture.vert.cso");
    D3D12_SHADER_BYTECODE pixelShaderObj = CreateCompiledShaderObjectFromPath("cso/basic_texture.frag.cso");

    do
    {
        if (vertexShaderObj.pShaderBytecode == nullptr || vertexShaderObj.BytecodeLength == 0) break;
        if (pixelShaderObj.pShaderBytecode == nullptr || pixelShaderObj.BytecodeLength == 0) break;

        // Define the vertex input layout used for Input Assembler
        const D3D12_INPUT_ELEMENT_DESC inputElementDescs[]{
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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

    return std::make_tuple(pipelineState, commandList, commandBundleList);;
}

static HBITMAP LoadBMPFile(BITMAP *outBitmapInfo)
{
    HBITMAP hBmp = (HBITMAP)LoadImageA(nullptr, "images/geom.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (hBmp == nullptr)
    {
        fprintf(stderr, "Load image file failed!\n");
        return nullptr;
    }

    BITMAP bitmap;
    if (GetObjectA(hBmp, sizeof(bitmap), &bitmap) == 0)
    {
        fprintf(stderr, "Bitmap get failed!\n");
        return nullptr;
    }

    if (outBitmapInfo != nullptr) {
        *outBitmapInfo = bitmap;
    }

    return hBmp;
}

// @return [cbv_srvDescriptorHeap, samplerDescriptorHeap, texture]
static auto CreateTextureAndSampler(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue,
                                    ID3D12CommandAllocator* commandAllocator, ID3D12GraphicsCommandList* commandList, ID3D12PipelineState* pipelineState) ->
                                    std::tuple <ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*>
{
    ID3D12DescriptorHeap* cbv_srvDescriptorHeap = nullptr;
    ID3D12DescriptorHeap* samplerDescriptorHeap = nullptr;
    ID3D12Resource* texture = nullptr;

    auto result = std::make_tuple(cbv_srvDescriptorHeap, samplerDescriptorHeap, texture);

    // Describe and create a constant buffer view & shader resource view (SRV) descriptor heap.
    const D3D12_DESCRIPTOR_HEAP_DESC cbv_srvHeapDesc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 2,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0
    };
    HRESULT hRes = d3d_device->CreateDescriptorHeap(&cbv_srvHeapDesc, IID_PPV_ARGS(&cbv_srvDescriptorHeap));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateDescriptorHeap for shader resource view failed: %ld\n", hRes);
        return result;
    }

    // Describe and create a sampler descriptor heap.
    const D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0
    };
    hRes = d3d_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&samplerDescriptorHeap));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateDescriptorHeap for sampler failed: %ld\n", hRes);
        return result;
    }

    const UINT cbv_srvDescriptorSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const UINT samplerDescriptorSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    HBITMAP hBmp = nullptr;
    ID3D12Resource* uploadDevHostBuffer = nullptr;
    bool done = false;

    // Create frame resources
    do
    {
        // Load BMP File
        BITMAP bitmap;
        hBmp = LoadBMPFile(&bitmap);

        const D3D12_HEAP_PROPERTIES defaultHeapProperties{
           .Type = D3D12_HEAP_TYPE_DEFAULT,
           .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
           .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
           .CreationNodeMask = 1,
           .VisibleNodeMask = 1
        };

        constexpr DXGI_FORMAT textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        const D3D12_RESOURCE_DESC textureResourceDesc{
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0,
            .Width = UINT64(bitmap.bmWidth),
            .Height = UINT64(bitmap.bmHeight),
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = textureFormat,
            .SampleDesc {.Count = 1, .Quality = 0U },
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
            .Flags = D3D12_RESOURCE_FLAG_NONE
        };

        const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
            .Format = textureFormat,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D {.MostDetailedMip = 0U, .MipLevels = 1U, .PlaneSlice = 0U, .ResourceMinLODClamp = 0.0f }
        };

        // Create the texture
        hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureResourceDesc, D3D12_RESOURCE_STATE_COMMON,
            nullptr, IID_PPV_ARGS(&texture));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommittedResource for texture failed: %ld!\n", hRes);
            break;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = cbv_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = cbvHandle;
        srvHandle.ptr += cbv_srvDescriptorSize;

        d3d_device->CreateShaderResourceView(texture, &srvDesc, srvHandle);

        // Upload image data to the texture
        const D3D12_HEAP_PROPERTIES uploadHeapProperties{
           .Type = D3D12_HEAP_TYPE_UPLOAD,     // for host visible memory which is used to upload data from host to device
           .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
           .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
           .CreationNodeMask = 1,
           .VisibleNodeMask = 1
        };

        const D3D12_RESOURCE_DESC texUploadResourceDesc{
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = UINT(bitmap.bmWidthBytes * bitmap.bmHeight),
            .Height = 1U,
            .DepthOrArraySize = 1U,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc {.Count = 1U, .Quality = 0 },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE
        };

        // Create uploadDevHostBuffer with host visible for upload
        hRes = d3d_device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &texUploadResourceDesc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadDevHostBuffer));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommittedResource for uploadDevHostBuffer failed: %ld\n", hRes);
            break;
        }

        void* hostMemPtr = nullptr;
        const D3D12_RANGE readRange{ 0, 0 };    // We do not intend to read from this resource on the CPU.
        hRes = uploadDevHostBuffer->Map(0, &readRange, &hostMemPtr);
        if (FAILED(hRes))
        {
            fprintf(stderr, "Map vertex buffer failed: %ld\n", hRes);
            break;
        }
        memcpy(hostMemPtr, bitmap.bmBits, size_t(bitmap.bmWidthBytes * bitmap.bmHeight));
        uploadDevHostBuffer->Unmap(0, nullptr);

        // Do texture upload
        WriteToDeviceTextureAndSync(commandList, texture, uploadDevHostBuffer, 0U, 0U, 0U, 0U, textureFormat, UINT(bitmap.bmWidth), UINT(bitmap.bmHeight), 1U, UINT(bitmap.bmWidthBytes));

        hRes = commandList->Close();
        if (FAILED(hRes))
        {
            fprintf(stderr, "Close command list failed: %ld\n", hRes);
            break;
        }

        // Execute the command list to complete the copy operation
        ID3D12CommandList* const ppCommandLists[] = { (ID3D12CommandList*)commandList };
        commandQueue->ExecuteCommandLists((UINT)std::size(ppCommandLists), ppCommandLists);

        WaitForPreviousFrame(commandQueue);
        if (!ResetCommandAllocatorAndList(commandAllocator, commandList, pipelineState)) break;

        // Create sampler resource
        const D3D12_SAMPLER_DESC samplerDesc{
            .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .MipLODBias = 0.0f,
            .MaxAnisotropy = 1U,
            .ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
            .BorderColor { 0.0f, 0.0f, 0.0f, 0.0f },
            .MinLOD = 0.0f,
            .MaxLOD = 1.0f
        };

        D3D12_CPU_DESCRIPTOR_HANDLE samplerHandle = samplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        d3d_device->CreateSampler(&samplerDesc, samplerHandle);
        samplerHandle.ptr += samplerDescriptorSize;

        done = true;
    }
    while (false);

    if (hBmp != nullptr) {
        DeleteObject(hBmp);
    }
    if (uploadDevHostBuffer != nullptr) {
        uploadDevHostBuffer->Release();
    }

    if (done) {
        return std::make_tuple(cbv_srvDescriptorHeap, samplerDescriptorHeap, texture);
    }

    if (cbv_srvDescriptorHeap != nullptr)
    {
        cbv_srvDescriptorHeap->Release();
        cbv_srvDescriptorHeap = nullptr;
    }
    if (samplerDescriptorHeap != nullptr)
    {
        samplerDescriptorHeap->Release();
        samplerDescriptorHeap = nullptr;
    }
    if (texture != nullptr)
    {
        texture->Release();
        texture = nullptr;
    }

    return std::make_tuple(cbv_srvDescriptorHeap, samplerDescriptorHeap, texture);
}

// @return std::make_tuple(uploadDevHostBuffer, vertexBuffer, cbvDescriptorHeap, rotateConstantBuffer)
static auto CreateVertexBuffer(ID3D12Device* d3d_device, ID3D12RootSignature* rootSignature, ID3D12CommandQueue* commandQueue,
                            ID3D12GraphicsCommandList* commandList, ID3D12GraphicsCommandList* commandBundleList,
                            ID3D12DescriptorHeap* cbv_srvDescriptorHeap, ID3D12DescriptorHeap* samplerDescriptorHeap, ID3D12Resource* texture) ->
                            std::tuple<ID3D12Resource*, ID3D12Resource*, ID3D12Resource*>
{
    const struct Vertex
    {
        float position[4];
        float texCoords[2];
    } squareVertices[]{
        // Direct3D是以左手作为前面背面顶点排列的依据
        // 由于BMP文件格式是从图像底部到顶部，因此需要将纹理坐标Y方向进行反转
        {.position { -0.75f, 0.75f, 0.0f, 1.0f }, .texCoords { 0.0f, 1.0f } },  // top left
        {.position { 0.75f, 0.75f, 0.0f, 1.0f }, .texCoords { 1.0f, 1.0f } },   // top right
        {.position { -0.75f, -0.75f, 0.0f, 1.0f }, .texCoords { 0.0f, 0.0f } }, // bottom left
        {.position { 0.75f, -0.75f, 0.0f, 1.0f }, .texCoords { 1.0f, 0.0f } }   // bottom right
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
    ID3D12Resource* rotateConstantBuffer = nullptr;

    auto result = std::make_tuple(uploadDevHostBuffer, vertexBuffer, rotateConstantBuffer);

    do
    {
        // Create vertexBuffer on GPU side.
        HRESULT hRes = d3d_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vbResourceDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&vertexBuffer));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommittedResource for vertex buffer failed: %ld\n", hRes);
            break;
        }

        // Create uploadDevHostBuffer with host visible for upload
        hRes = d3d_device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vbResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadDevHostBuffer));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommittedResource for uploadDevHostBuffer failed: %ld\n", hRes);
            break;
        }

        // Create rotate constant buffer object
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

        hRes = d3d_device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &cbResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&rotateConstantBuffer));
        if (FAILED(hRes))
        {
            fprintf(stderr, "CreateCommittedResource for rotate constant buffer failed: %ld\n", hRes);
            break;
        }

        void* hostMemPtr = nullptr;
        const D3D12_RANGE readRange{ 0, 0 };    // We do not intend to read from this resource on the CPU.
        hRes = uploadDevHostBuffer->Map(0, &readRange, &hostMemPtr);
        if (FAILED(hRes))
        {
            fprintf(stderr, "Map vertex buffer failed: %ld\n", hRes);
            break;
        }

        memcpy(hostMemPtr, squareVertices, sizeof(squareVertices));
        uploadDevHostBuffer->Unmap(0, nullptr);

        WriteToDeviceResourceAndSync(commandList, vertexBuffer, uploadDevHostBuffer, 0U, 0U, sizeof(squareVertices));

        hRes = commandList->Close();
        if (FAILED(hRes))
        {
            fprintf(stderr, "Close command list failed: %ld\n", hRes);
            break;
        }

        // Clear the constant buffer
        hRes = rotateConstantBuffer->Map(0, &readRange, &hostMemPtr);
        if (FAILED(hRes))
        {
            fprintf(stderr, "Map constant buffer failed: %ld\n", hRes);
            break;
        }
        memset(hostMemPtr, 0, CONSTANT_BUFFER_ALLOCATION_GRANULARITY);
        rotateConstantBuffer->Unmap(0, nullptr);

        // Execute the command list to complete the copy operation
        ID3D12CommandList* const ppCommandLists[] = { (ID3D12CommandList*)commandList };
        commandQueue->ExecuteCommandLists((UINT)std::size(ppCommandLists), ppCommandLists);

        // Initialize the vertex buffer view.
        const D3D12_VERTEX_BUFFER_VIEW vertexBufferView{
            .BufferLocation = vertexBuffer->GetGPUVirtualAddress(),
            .SizeInBytes = (uint32_t)sizeof(squareVertices),
            .StrideInBytes = sizeof(squareVertices[0])
        };

        auto const descHandleIncrSize = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        const D3D12_CONSTANT_BUFFER_VIEW_DESC rotateCBVDesc{
            .BufferLocation = rotateConstantBuffer->GetGPUVirtualAddress(),
            .SizeInBytes = CONSTANT_BUFFER_ALLOCATION_GRANULARITY
        };
        D3D12_CPU_DESCRIPTOR_HANDLE rotateCBVCPUDescHandle = cbv_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        d3d_device->CreateConstantBufferView(&rotateCBVDesc, rotateCBVCPUDescHandle);

        // Fetch CBV and UAV CPU descriptor handles
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
        D3D12_CPU_DESCRIPTOR_HANDLE textureSRVCPUDescHandle = rotateCBVCPUDescHandle;
        textureSRVCPUDescHandle.ptr += descHandleIncrSize;
        d3d_device->CreateShaderResourceView(texture, &textureSRVDesc, textureSRVCPUDescHandle);

        // Fetch SRV, CBV, and sampler GPU descriptor handles
        D3D12_GPU_DESCRIPTOR_HANDLE constBufferDescHandler = cbv_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE textureSRVGPUDescHandle = constBufferDescHandler;
        textureSRVGPUDescHandle.ptr += descHandleIncrSize;
        D3D12_GPU_DESCRIPTOR_HANDLE samplerDescHandler = samplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

        // Record commands to the command list bundle.
        commandBundleList->SetGraphicsRootSignature(rootSignature);
        ID3D12DescriptorHeap* const descHeaps[]{ cbv_srvDescriptorHeap, samplerDescriptorHeap };
        // ATTENTION: SetDescriptorHeaps should be set into command bundle list as well as command list
        commandBundleList->SetDescriptorHeaps(UINT(std::size(descHeaps)), descHeaps);
        commandBundleList->SetGraphicsRootDescriptorTable(0, textureSRVGPUDescHandle);  // rootParameters[0]
        commandBundleList->SetGraphicsRootDescriptorTable(1, samplerDescHandler);       // rootParameters[1]
        commandBundleList->SetGraphicsRootDescriptorTable(2, constBufferDescHandler);   // rootParameters[2]
        commandBundleList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        commandBundleList->IASetVertexBuffers(0, 1, &vertexBufferView);

        commandBundleList->DrawInstanced((UINT)std::size(squareVertices), 1U, 0U, 0U);

        // End of the record
        hRes = commandBundleList->Close();
        if (FAILED(hRes))
        {
            fprintf(stderr, "Close basic command bundle failed: %ld\n", hRes);
            break;
        }

        // Wait for the command list to execute;
        // we are reusing the same command list in our main loop but for now,
        // we just want to wait for setup to complete before continuing.
        WaitForPreviousFrame(commandQueue);
    }
    while (false);

    return std::make_tuple(uploadDevHostBuffer, vertexBuffer, rotateConstantBuffer);
}

auto CreateTextureBasicTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue *commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                    std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>
{
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12GraphicsCommandList* commandBundle = nullptr;
    ID3D12DescriptorHeap* cbv_srvDescriptorHeap = nullptr;
    ID3D12DescriptorHeap* samplerDescriptorHeap = nullptr;
    ID3D12Resource* uploadDevHostBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    ID3D12Resource* texture = nullptr;
    ID3D12Resource* constantBuffer = nullptr;
    bool success = false;
    
    auto const result = std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, cbv_srvDescriptorHeap, samplerDescriptorHeap, uploadDevHostBuffer, vertexBuffer, texture, constantBuffer, success);

    rootSignature = CreateRootSignature(d3d_device);
    if (rootSignature == nullptr) return result;

    success = true;

    auto const pipelinePresentResult = CreatePipelineStateObject(d3d_device, commandAllocator, commandBundleAllocator, rootSignature);
    pipelineState = std::get<0>(pipelinePresentResult);
    commandList = std::get<1>(pipelinePresentResult);
    commandBundle = std::get<2>(pipelinePresentResult);

    if (pipelineState == nullptr || commandList == nullptr || commandBundle == nullptr) {
        success = false;
    }

    auto const textureResult = CreateTextureAndSampler(d3d_device, commandQueue, commandAllocator, commandList, pipelineState);
    cbv_srvDescriptorHeap = std::get<0>(textureResult);
    samplerDescriptorHeap = std::get<1>(textureResult);
    texture = std::get<2>(textureResult);

    auto const vertexBufferResult = CreateVertexBuffer(d3d_device, rootSignature, commandQueue, commandList, commandBundle, cbv_srvDescriptorHeap, samplerDescriptorHeap, texture);
    uploadDevHostBuffer = std::get<0>(vertexBufferResult);
    vertexBuffer = std::get<1>(vertexBufferResult);
    constantBuffer = std::get<2>(vertexBufferResult);

    return std::make_tuple(rootSignature, pipelineState, commandList, commandBundle, cbv_srvDescriptorHeap, samplerDescriptorHeap, uploadDevHostBuffer, vertexBuffer, texture, constantBuffer, success);
}

