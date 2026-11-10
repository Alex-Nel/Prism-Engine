#include "Vector3.h"
#include <math.h>



///////////////////////////
// Functions for Vector3 //
///////////////////////////

//
// Adding two vectors.
//
Vector3 Vector3Add(Vector3 a, Vector3 b)
{
    Vector3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}



//
// Subtracing two vectors.
//
Vector3 Vector3Subtract(Vector3 a, Vector3 b)
{
    Vector3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}



//
// Cross product of two vectors.
//
Vector3 Vector3Cross(Vector3 a, Vector3 b)
{
    Vector3 result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;

    return result;
}



//
// Component-wise multiplication.
//
Vector3 Vector3Multiply(Vector3 a, Vector3 b)
{
    Vector3 result = {
        a.x * b.x,
        a.y * b.y,
        a.z * b.z
    };

    return result;
}



//
// Divide a vector by a scalar.
//
Vector3 Vector3Divide(Vector3 v, float s)
{
    if (s == 0.0f)
    {
        Vector3 zero = {0, 0, 0};
        return zero;
    }

    Vector3 result = {
        v.x / s,
        v.y / s,
        v.z / s
    };

    return result;
}



//
// Normalizing a vector to it's base form.
//
Vector3 Vector3Normalize(Vector3 v)
{
    float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);

    if (length == 0.0f) 
    {
        Vector3 v = {0, 0, 0};
        return v; // avoid division by zero
    }

    Vector3 result;
    result.x = v.x / length;
    result.y = v.y / length;
    result.z = v.z / length;

    return result;
}



//
// Dot product of two vectors.
//
float Vector3Dot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}



//
// Scaling a vector by a scalar float.
//
Vector3 Vector3Scale(Vector3 v, float s)
{
    Vector3 vo = {v.x * s, v.y * s, v.z * s};
    return vo;
}



//
// Negates a vector (inverts all components).
//
Vector3 Vector3Negate(Vector3 v)
{
    Vector3 result = {
        -v.x,
        -v.y,
        -v.z
    };

    return result;
}



//
// Length (magnitude) of a vector.
//
float Vector3Length(Vector3 v)
{
    return sqrtf(Vector3Dot(v, v));
}



//
// Linearly interpolate a vector from one point to another given a time period.
//
Vector3 Vector3Lerp(Vector3 start, Vector3 end, float t)
{
    Vector3 v = {
        start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t,
        start.z + (end.z - start.z) * t
    };

    return v;
}



//
// Spherical Linear Interpolation for Vector3
//
Vector3 Vector3Slerp(Vector3 start, Vector3 end, float t)
{
    // Calculate the magnitudes (lengths) of both vectors
    float lenStart = sqrtf(start.x * start.x + start.y * start.y + start.z * start.z);
    float lenEnd   = sqrtf(end.x * end.x + end.y * end.y + end.z * end.z);

    // If either vector is zero-length, fallback to standard Lerp
    if (lenStart < 0.0001f || lenEnd < 0.0001f)
        return Vector3Lerp(start, end, t);

    // Normalize copies of the vectors to find the pure angular difference
    Vector3 nStart = { start.x / lenStart, start.y / lenStart, start.z / lenStart };
    Vector3 nEnd   = { end.x / lenEnd,     end.y / lenEnd,     end.z / lenEnd };

    // Dot product of normalized vectors gives the cosine of the angle between them
    float cosTheta = nStart.x * nEnd.x + nStart.y * nEnd.y + nStart.z * nEnd.z;

    // Clamp to avoid floating-point inaccuracies pushing cosTheta out of [-1, 1]
    cosTheta = fmaxf(-1.0f, fminf(cosTheta, 1.0f));

    // If vectors are nearly parallel (collinear), fallback to Lerp to avoid division by zero
    if (cosTheta > 0.9995f)
        return Vector3Lerp(start, end, t);

    // Calculate the angle and its sine
    float theta = acosf(cosTheta);
    float sinTheta = sinf(theta);

    // Compute Slerp scale factors for the direction
    float ratioA = sinf((1.0f - t) * theta) / sinTheta;
    float ratioB = sinf(t * theta) / sinTheta;

    // Interpolate the direction
    Vector3 resultDir = {
        (nStart.x * ratioA) + (nEnd.x * ratioB),
        (nStart.y * ratioA) + (nEnd.y * ratioB),
        (nStart.z * ratioA) + (nEnd.z * ratioB)
    };

    // Linearly interpolate the magnitudes (lengths) 
    float targetLength = lenStart + (lenEnd - lenStart) * t;

    // Scale the resulting direction by the interpolated magnitude
    Vector3 finalVector = {
        resultDir.x * targetLength,
        resultDir.y * targetLength,
        resultDir.z * targetLength
    };

    return finalVector;
}



//
// Distance between two points.
//
float Vector3Distance(Vector3 a, Vector3 b)
{
    return Vector3Length(Vector3Subtract(a, b));
}