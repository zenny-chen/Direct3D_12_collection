/** Model view translation matrix *
 * [ 1  0  0  0
     0  1  0  0
     0  0  1  0
     x  y  z  1
 * ]
*/

/** Ortho projection matrix *
 * [ 2/(r-l)       0             0             0
     0             2/(t-b)       0             0
     0             0             -2/(f-n)      0
     -(r+l)/(r-l)  -(t+b)/(t-b)  -(f+n)/(f-n)  1
 * ]
*/

/** rotate matrix *
 * [x^2*(1-c)+c  xy*(1-c)+zs  xz(1-c)-ys  0
    xy(1-c)-zs   y^2*(1-c)+c  yz(1-c)+xs  0
    xz(1-c)+ys   yz(1-c)-xs   z^2(1-c)+c  0
    0            0            0           1
 * ]
 * |(x, y, z)| must be 1.0
*/

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

// Old declaration
//cbuffer cbRotationAngle : register(b0)
// In Shader Model 5.1, we can use
struct CBRotationAngle
{
    float rotAngle;
    int paddings[64 - 1];
};

struct CBTranslateOffset
{
    float xOffset;
    float yOffset;
    int paddings[64 - 2];
};

ConstantBuffer<CBTranslateOffset> cbTranslateOffset : register(b0);
ConstantBuffer<CBRotationAngle> cbRotationAngle : register(b1);

// vertexIndex is a system-value input parameter and
// shadingRate is a system-value output parameter
PSInput VSMain(float4 position : POSITION, float4 color : COLOR, uint vertexIndex : SV_VertexID, out uint shadingRate : SV_ShadingRate)
{
    // glTranslate(xOffset, yOffset, -2.3, 1.0)
    const float4x4 translateMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,     // row 0
        0.0f, 1.0f, 0.0f, 0.0f,     // row 1
        0.0f, 0.0f, 1.0f, 0.0f,     // row 2
        cbTranslateOffset.xOffset, cbTranslateOffset.yOffset, -2.3f, 1.0f     // row 3
    };

    const float rotRadian = radians(cbRotationAngle.rotAngle);

    // glRotate(u_angle, 0.0, 0.0, 1.0)
    const float4x4 rotateMatrix = {
        cos(rotRadian), sin(rotRadian), 0.0f, 0.0f,     // row 0
        -sin(rotRadian), cos(rotRadian), 0.0f, 0.0f,    // row 1
        0.0f, 0.0f, 1.0f, 0.0f,                         // row 2
        0.0f, 0.0f, 0.0f, 1.0f                          // row 3
    };

    // glOrtho(-1.0, 1.0, -1.0, 1.0, 1.0, 3.0)
    const float4x4 projectionMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,     // row 0
        0.0f, 1.0f, 0.0f, 0.0f,     // row 1
        0.0f, 0.0f, -1.0f, 0.0f,    // row 2
        0.0f, 0.0f, -2.0f, 1.0f     // row 3
    };

    PSInput result;
    result.position = mul(position, mul(rotateMatrix, mul(translateMatrix, projectionMatrix)));
    result.color = color;

    enum D3D12_SHADING_RATE
    {
        D3D12_SHADING_RATE_1X1 = 0,
        D3D12_SHADING_RATE_1X2 = 0x1,
        D3D12_SHADING_RATE_2X1 = 0x4,
        D3D12_SHADING_RATE_2X2 = 0x5,
        D3D12_SHADING_RATE_2X4 = 0x6,
        D3D12_SHADING_RATE_4X2 = 0x9,
        D3D12_SHADING_RATE_4X4 = 0xa
    };

    const D3D12_SHADING_RATE shadingRates[] = {
        D3D12_SHADING_RATE_1X1,
        D3D12_SHADING_RATE_1X2,
        D3D12_SHADING_RATE_2X1,
        D3D12_SHADING_RATE_2X2,
        D3D12_SHADING_RATE_2X4,
        D3D12_SHADING_RATE_4X2,
        D3D12_SHADING_RATE_4X4
    };
    shadingRate = D3D12_SHADING_RATE_1X2;    // shadingRates[vertexIndex];

    return result;
}

