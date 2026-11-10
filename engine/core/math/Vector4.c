#include "Vector4.h"
#include <math.h>



//
// Adds two Vector4's
//
Vector4 Vector4Add(Vector4 a, Vector4 b)
{
    Vector4 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    result.w = a.w + b.w;

    return result;
}



//
// Subtracts two Vector4's
//
Vector4 Vector4Subtract(Vector4 a, Vector4 b)
{
    Vector4 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    result.w = a.w - b.w;

    return result;
}



//
// Component-wise multiplication.
//
Vector4 Vector4Multiply(Vector4 a, Vector4 b)
{
    Vector4 result =
    {
        a.x * b.x,
        a.y * b.y,
        a.z * b.z,
        a.w * b.w
    };

    return result;
}



//
// Divide a vector by a scalar.
//
Vector4 Vector4Divide(Vector4 v, float s)
{
    if (s == 0.0f)
    {
        Vector4 zero = {0, 0, 0, 0};
        return zero;
    }

    Vector4 result = {
        v.x / s,
        v.y / s,
        v.z / s,
        v.w / s
    };

    return result;
}



//
// Normalizing a Vector4 to it's base form
//
Vector4 Vector4Normalize(Vector4 v)
{
    float length = sqrtf( v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w );

    if (length == 0.0f)
    {
        Vector4 zero = {0, 0, 0, 0};
        return zero; // avoid division by zero
    }

    Vector4 result;
    result.x = v.x / length;
    result.y = v.y / length;
    result.z = v.z / length;
    result.w = v.w / length;

    return result;
}



//
// Dot product of two Vector4's
//
float Vector4Dot(Vector4 a, Vector4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}



//
// Scaling a vector by a scalar
//
Vector4 Vector4Scale(Vector4 v, float s)
{
    Vector4 result = {
        v.x * s,
        v.y * s,
        v.z * s,
        v.w * s
    };

    return result;
}



//
// Negate a vector (inverts all components).
//
Vector4 Vector4Negate(Vector4 v)
{
    Vector4 result = {
        -v.x,
        -v.y,
        -v.z,
        -v.w
    };

    return result;
}



//
// Length (magnitude) of a vector.
//
float Vector4Length(Vector4 v)
{
    return sqrtf(Vector4Dot(v, v));
}



//
// Linearly interpolate a Vector4
//
Vector4 Vector4Lerp(Vector4 start, Vector4 end, float t)
{
    Vector4 result =
    {
        start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t,
        start.z + (end.z - start.z) * t,
        start.w + (end.w - start.w) * t
    };

    return result;
}



//
// Distance between two points.
//
float Vector4Distance(Vector4 a, Vector4 b)
{
    return Vector4Length(Vector4Subtract(a, b));
}