#pragma once

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstdarg>
#include <cassert>
#include <cerrno>

#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>
#include <limits>
#include <algorithm>
#include <utility>
#include <array>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>


#define USE_MSAA_RENDER_TARGET      0

struct alignas(sizeof(void*)) RootSignatureSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE rootSignatureSubType;
    ID3D12RootSignature* rootSignature;
};

struct alignas(sizeof(void*)) ShaderByteCodeSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE shaderObjSubType;
    D3D12_SHADER_BYTECODE shaderByteCode;
};

struct alignas(sizeof(void*)) BlendStateSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE blendSubType;
    D3D12_BLEND_DESC blendDesc;
};

struct alignas(sizeof(void*)) SampleMaskSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE sampleMaskSubType;
    UINT sampleMask;
};

struct alignas(sizeof(void*)) RasterizerStateSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE rasterizerSubType;
    D3D12_RASTERIZER_DESC rasterizerState;
};

struct alignas(sizeof(void*)) DepthStencilSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE depthStencilSubType;
    D3D12_DEPTH_STENCIL_DESC depthStencilState;
};

struct alignas(sizeof(void*)) IBStripCutValueSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE ibStripCutValueSubType;
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ibStripCutValue;
};

struct alignas(sizeof(void*)) PrimitiveTopologyTypeSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE primitiveSubType;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType;
};

struct alignas(sizeof(void*)) RenderTargetFormatsSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE renderTargetFormatSubType;
    D3D12_RT_FORMAT_ARRAY renderTargetFormats;
};

struct alignas(sizeof(void*)) DepthStencilViewFormat
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE depthStencilSubType;
    DXGI_FORMAT depthStencilViewFormat;
};

struct alignas(sizeof(void*)) SampleDescSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE sampleSubType;
    DXGI_SAMPLE_DESC sampleDesc;
};

struct alignas(sizeof(void*)) NodeMaskSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE nodeMaskSubType;
    UINT nodeMask;
};

struct alignas(sizeof(void*)) CachedPSOSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE cachedPSOSubType;
    D3D12_CACHED_PIPELINE_STATE cachedPSO;
};

struct alignas(sizeof(void*)) FlagsSubobject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE flagsSubType;
    D3D12_PIPELINE_STATE_FLAGS flags;
};

enum MeshShaderExecMode
{
    BASIC_MODE,
    ONLY_MESH_SHADER_MODE
};

enum TranslationType
{
    ROTATE_CLOCKWISE,
    ROTATE_COUNTER_CLOCKWISE,
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_UP,
    MOVE_DOWN,
    MOVE_NEAR,
    MOVE_FAR
};

struct CommonTranslationSet
{
    float rotAngle;
    float xOffset;
    float yOffset;
    float zOffset;
};

// Window Width
static constexpr int WINDOW_WIDTH = 512;

// Window Height
static constexpr int WINDOW_HEIGHT = 512;

// Viewport Width
static constexpr UINT VIEWPORT_WIDTH = 512U;

// Viewport Height
static constexpr UINT VIEWPORT_HEIGHT = 512U;

// Defined by the Direct3D 12 Spec (In bytes)
static constexpr UINT CONSTANT_BUFFER_ALLOCATION_GRANULARITY = 256U;

// Default swap-chain buffer and render target buffer format
static constexpr DXGI_FORMAT RENDER_TARGET_BUFFER_FOMRAT = DXGI_FORMAT_R8G8B8A8_UNORM;

extern auto CreateCompiledShaderObjectFromPath(const char csoPath[]) -> D3D12_SHADER_BYTECODE;

// Used to sync commandQueue->ExecuteCommandLists 
extern auto WaitForPreviousFrame(ID3D12CommandQueue* commandQueue) -> bool;

extern auto ResetCommandAllocatorAndList(ID3D12CommandAllocator* commandAllocator, ID3D12GraphicsCommandList* commandList, ID3D12PipelineState* pipelineState) -> bool;

extern auto WriteToDeviceResourceAndSync(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    size_t dstOffset,
    size_t srcOffset,
    size_t dataSize) -> void;

extern auto WriteToDeviceTextureAndSync(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    UINT dstX,
    UINT dstY,
    UINT dstZ,
    size_t srcOffset,
    DXGI_FORMAT textureFormat,
    UINT width,
    UINT height,
    UINT depth,
    UINT rowPitch) -> void;

extern auto SyncAndReadFromDeviceResource(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    size_t dataSize,
    _In_ ID3D12Resource* pDestinationHostBuffer,
    _In_ ID3D12Resource* pSourceUAVBuffer) -> void;

extern auto CreateTextureBasicTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>;

extern auto RenderPostProcessForTransformFeedback() -> void;
extern auto CreateTransformFeedbackTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*>;

extern auto ProjectionTestTranslateProcess(const TranslationType& transType) -> void;
extern auto ProjectionTestFetchTranslationSet() -> CommonTranslationSet;
extern auto CreateProjectionTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>;

extern auto CreateMeshShaderTestAssets(MeshShaderExecMode execMode, ID3D12Device* d3d_device, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*>;

extern auto CreateMeshShaderNoRasterTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*, ID3D12DescriptorHeap*>;

extern auto CreateVariableRateShadingTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>;

extern auto CreateConservativeRasterizationTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>;

extern auto CreateExecuteIndirectTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator, bool supportMeshShader) ->
                                        std::tuple<ID3D12RootSignature*, std::array<ID3D12PipelineState*, 3>, ID3D12GraphicsCommandList*, std::array<ID3D12GraphicsCommandList*, 3>, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, std::array<ID3D12CommandSignature*, 3>, bool>;

extern auto ExecuteIndirectCallbackHandler(ID3D12GraphicsCommandList* commandList, ID3D12CommandSignature* commandSignature, ID3D12Resource* indirectArgumentBuffer, ID3D12Resource* indirectCountBuffer, UINT index) -> void;

extern auto CreatePSWritePrimIDTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>;

extern auto CreateDepthBoundTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>;

extern auto CreateTargetIndependentTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>;

extern auto CreateGeometryShaderTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*, bool>;

extern auto CreateGeneralRasterizationTestAssets(ID3D12Device* d3d_device, ID3D12CommandQueue* commandQueue, ID3D12CommandAllocator* commandAllocator, ID3D12CommandAllocator* commandBundleAllocator) ->
                                        std::tuple<ID3D12RootSignature*, ID3D12PipelineState*, ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*, ID3D12DescriptorHeap*, ID3D12DescriptorHeap*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, bool>;

