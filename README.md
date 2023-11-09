# Direct3D_12_collection
Direct3D 12 Usage Collection

<br />

This project was built with Visual Studio 2022 Community Edition on Windows 11 platform.

<br />

# Combining shading rate factors with D3D12_SHADING_RATE_COMBINER_SUM

A/B | 1x1 | 1x2 | 2x1 | 2x2 | 2x4 | 4x2 | 4x4
---- | ---- | ---- | ---- | ---- | ---- | ---- | ----
1x1 | 1x1 | 1x2 | 2x1 | 2x2 | 2x4 | 4x2 | 4x4
1x2 | 1x2 | 1x1 | 2x2 | 2x4 | 1x1 | 4x4 | 4x4
2x1 | 2x1 | 2x2 | 1x1 | 4x2 | 4x4 | 4x4 | 4x4
2x2 | 2x2 | 2x4 | 4x2 | 4x4 | 4x4 | 4x4 | 4x4
2x4 | 2x4 | 1x1 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4
4x2 | 4x2 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4
4x4 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4


