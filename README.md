# Direct3D_12_collection
Direct3D 12 Usage Collection

<br />

This project was built with Visual Studio 2022 Community Edition on Windows 11 platform.

<br />

# GTX 1650 Shading Rate Combiner Operation

<br />

## Combining shading rate factors (width x height) with D3D12_SHADING_RATE_COMBINER_SUM

A/B | 1x1 | 1x2 | 2x1 | 2x2 | 2x4 | 4x2 | 4x4
---- | ---- | ---- | ---- | ---- | ---- | ---- | ----
1x1 | 1x1 | 1x2 | 2x1 | 2x2 | 2x4 | 4x2 | 4x4
1x2 | 1x2 | 1x1 | 2x2 | 2x4 | 1x1 | 4x4 | 4x4
2x1 | 2x1 | 2x2 | 1x1 | 4x2 | 4x4 | 4x4 | 4x4
2x2 | 2x2 | 2x4 | 4x2 | 4x4 | 4x4 | 4x4 | 4x4
2x4 | 2x4 | 1x1 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4
4x2 | 4x2 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4
4x4 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4 | 4x4

<br />

## Algorithm

```cpp
enum D3D12_SHADING_RATE
{
    D3D12_SHADING_RATE_1X1	= 0,
    D3D12_SHADING_RATE_1X2	= 0x1,
    D3D12_SHADING_RATE_2X1	= 0x4,
    D3D12_SHADING_RATE_2X2	= 0x5,
    D3D12_SHADING_RATE_2X4	= 0x6,
    D3D12_SHADING_RATE_4X2	= 0x9,
    D3D12_SHADING_RATE_4X4	= 0xa
};

unsigned SUMcombiner(enum D3D12_SHADING_RATE a, enum D3D12_SHADING_RATE b)
{
    unsigned c = a + b;
    if(c > D3D12_SHADING_RATE_4X4) {
        c = D3D12_SHADING_RATE_4X4;
    }

    switch(c)
    {
        case D3D12_SHADING_RATE_1X1:
        default:
            return D3D12_SHADING_RATE_1X1;

        case D3D12_SHADING_RATE_1X2:
            return D3D12_SHADING_RATE_1X2;

        case D3D12_SHADING_RATE_2X1:
            return D3D12_SHADING_RATE_2X1;

        case D3D12_SHADING_RATE_2X2:
            return D3D12_SHADING_RATE_2X2;

        case D3D12_SHADING_RATE_2X4:
            return D3D12_SHADING_RATE_2X4;

        case D3D12_SHADING_RATE_4X2:
            return D3D12_SHADING_RATE_4X2;

        case D3D12_SHADING_RATE_4X4:
            return D3D12_SHADING_RATE_4X4;
    }
}
```

