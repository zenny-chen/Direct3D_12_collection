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

/** Frustum perspetive projection matrix *
 * [ 2n/(r-l)       0             0             0
     0             2n/(t-b)       0             0
    (r+l)/(r-l)   (t+b)/(t-b)  -(f+n)/(f-n)     -1
     0              0          -2fn/(f-n)       0
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
    linear float4 color : COLOR;
};

// Old declaration
// cbuffer CBTranslations : register(b0)
// In Shader Model 5.1, we can use
struct CBTranslations
{
    float rotAngle;
    float xOffset;
    float yOffset;
    float zOffset;
    
    int paddings[64 - 4];
};

ConstantBuffer<CBTranslations> cbTranslations : register(b0, space0);

// vertexIndex is a system-value input parameter and
// shadingRate is a system-value output parameter
PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    // glTranslate(xOffset, yOffset, zOffset, 1.0)
    const float4x4 translateMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,                                                         // row 0
        0.0f, 1.0f, 0.0f, 0.0f,                                                         // row 1
        0.0f, 0.0f, 1.0f, 0.0f,                                                         // row 2
        cbTranslations.xOffset, cbTranslations.yOffset, cbTranslations.zOffset, 1.0f    // row 3
    };

#if 0
    // glOrtho(-1.0, 1.0, -1.0, 1.0, 1.0, 9.0)
    const float4x4 projectionMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,         // row 0
        0.0f, 1.0f, 0.0f, 0.0f,         // row 1
        0.0f, 0.0f, -0.25f, 0.0f,       // row 2
        0.0f, 0.0f, -1.25f, 1.0f        // row 3
    };
#else
    // glFrustum(-1.0, 1.0, -1.0, 1.0, 1.0, 9.0)
    const float4x4 projectionMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,         // row 0
        0.0f, 1.0f, 0.0f, 0.0f,         // row 1
        0.0f, 0.0f, -1.25f, -1.0f,      // row 2
        0.0f, 0.0f, -2.25f, 0.0f        // row 3
    };
#endif

    const float rotRadian = radians(cbTranslations.rotAngle);

    // glRotate(radian, 1.0, 0.0, 0.0)
    const float4x4 rotateMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, cos(rotRadian), sin(rotRadian), 0.0f,
        0.0f, -sin(rotRadian), cos(rotRadian), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    PSInput result;
    result.position = mul(position, mul(rotateMatrix, mul(translateMatrix, projectionMatrix)));
    result.color = color;

    return result;
}

