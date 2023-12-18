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
};

ConstantBuffer<CBRotationAngle> cbRotationAngle : register(b0);

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    const int instanceID = int(position.w);
    position.w = 1.0f;

    float xOffset = 0.0f;
    float yOffset = 0.0f;
    switch (instanceID)
    {
    case 0:
        xOffset = -0.65f;
        yOffset = 0.0f;
        break;

    case 1:
        xOffset = 0.0f;
        yOffset = 0.65f;
        break;

    case 2:
        xOffset = 0.65f;
        yOffset = 0.0f;
        break;

    case 3:
        xOffset = 0.0f;
        yOffset = -0.65f;
        break;

    default:
        break;
    }

    // glTranslate(xOffset, yOffset, -7.0, 1.0)
    const float4x4 translateMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,     // row 0
        0.0f, 1.0f, 0.0f, 0.0f,     // row 1
        0.0f, 0.0f, 1.0f, 0.0f,     // row 2
        xOffset, yOffset, -7.0f, 1.0f     // row 3
    };

    float rotRadian = radians(cbRotationAngle.rotAngle);

    float4x4 rotateMatrix = translateMatrix;

    if (instanceID != 1 && instanceID != 2)
    {
        if (instanceID == 3) {
            rotRadian = -rotRadian;
        }

        // glRotate(rotAngle, 0.0, 0.0, 1.0)
        const float4x4 rotMatrix = {
            cos(rotRadian), sin(rotRadian), 0.0f, 0.0f,     // row 0
            -sin(rotRadian), cos(rotRadian), 0.0f, 0.0f,    // row 1
            0.0f, 0.0f, 1.0f, 0.0f,                         // row 2
            0.0f, 0.0f, 0.0f, 1.0f                          // row 3
        };
        rotateMatrix = rotMatrix;
    }
    else if (instanceID == 1)
    {
        // glRotate(rotAngle, 0.0, 1.0, 0.0)
        const float4x4 rotMatrix = {
            cos(rotRadian), 0.0f, -sin(rotRadian), 0.0f,    // row 0
            0.0f, 1.0f, 0.0f, 0.0f,                         // row 1
            sin(rotRadian), 0.0f, cos(rotRadian), 0.0f,     // row 2
            0.0f, 0.0f, 0.0f, 1.0f                          // row 3
        };
        rotateMatrix = rotMatrix;
    }
    else if (instanceID == 2)
    {
        // glRotate(rotAngle, 1.0, 0.0, 0.0)
        const float4x4 rotMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, cos(rotRadian), sin(rotRadian), 0.0f,
            0.0f, -sin(rotRadian), cos(rotRadian), 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        rotateMatrix = rotMatrix;
    }

    // glOrtho(-1.0, 1.0, -1.0, 1.0, 1.0, 9.0)
    const float4x4 projectionMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,     // row 0
        0.0f, 1.0f, 0.0f, 0.0f,     // row 1
        0.0f, 0.0f, -0.25f, 0.0f,   // row 2
        0.0f, 0.0f, -1.25f, 1.0f    // row 3
    };

    PSInput result;
    result.position = mul(position, mul(rotateMatrix, mul(translateMatrix, projectionMatrix)));
    result.color = color;

    return result;
}

