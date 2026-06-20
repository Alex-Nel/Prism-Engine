#include "Vector2.h"
#include <math.h>


///////////////////////////
// Functions for Vector2 //
///////////////////////////


//
// Adding two Vector2's
//
Vector2 Vector2Add(Vector2 a, Vector2 b)
{
    Vector2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}



//
// Subtracting two vector2's
//
Vector2 Vector2Subtract(Vector2 a, Vector2 b)
{
    Vector2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}



//
// 2D "Cross Product" (Returns the determinant/perp-dot product scalar)
//
float Vector2Cross(Vector2 a, Vector2 b)
{
    return a.x * b.y - a.y * b.x;
}



//
// Component-wise multiplication.
//
Vector2 Vector2Multiply(Vector2 a, Vector2 b)
{
    Vector2 result = {
        a.x * b.x,
        a.y * b.y
    };
    return result;
}



//
// Divide a vector by a scalar.
//
Vector2 Vector2Divide(Vector2 v, float s)
{
    if (s == 0.0f)
    {
        Vector2 zero = {0, 0};
        return zero;
    }

    Vector2 result = {
        v.x / s,
        v.y / s
    };
    return result;
}



//
// Normalize a vector2 to it's base form
//
Vector2 Vector2Normalize(Vector2 v)
{
    float length = sqrtf(v.x * v.x + v.y * v.y);
    
    if (length == 0.0f) 
    {
        Vector2 r = {0, 0};
        return r;
    }

    Vector2 result;
    result.x = v.x / length;
    result.y = v.y / length;

    return result;
}



//
// Cross product of 2 vectors
//
float Vector2Dot(Vector2 a, Vector2 b)
{
    return a.x * b.x + a.y * b.y;
}



//
// Scaling a vector2 by a scalar
//
Vector2 Vector2Scale(Vector2 v, float s)
{
    Vector2 vo = {v.x * s, v.y * s};
    return vo;
}



//
// Negates a vector (inverts all components).
//
Vector2 Vector2Negate(Vector2 v)
{
    Vector2 result = {
        -v.x,
        -v.y
    };
    return result;
}



//
// Length (magnitude) of a vector.
//
float Vector2Length(Vector2 v)
{
    return sqrtf(Vector2Dot(v, v));
}



//
// Linearly interpolate from a start and end given an amount of time
//
Vector2 Vector2Lerp(Vector2 start, Vector2 end, float t)
{
    Vector2 v = {
        start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t
    };

    return v;
}



//
// Spherical Linear Interpolation for Vector2
//
Vector2 Vector2Slerp(Vector2 start, Vector2 end, float t)
{
    // Calculate the magnitudes (lengths) of both vectors
    float lenStart = sqrtf(start.x * start.x + start.y * start.y);
    float lenEnd   = sqrtf(end.x * end.x + end.y * end.y);

    // If either vector is zero-length, fallback to standard Lerp
    if (lenStart < 0.0001f || lenEnd < 0.0001f)
        return Vector2Lerp(start, end, t);

    // Normalize copies of the vectors to find the pure angular difference
    Vector2 nStart = { start.x / lenStart, start.y / lenStart };
    Vector2 nEnd   = { end.x / lenEnd,     end.y / lenEnd };

    // Dot product of normalized vectors gives the cosine of the angle between them
    float cosTheta = nStart.x * nEnd.x + nStart.y * nEnd.y;

    // Clamp to avoid floating-point inaccuracies pushing cosTheta out of [-1, 1]
    cosTheta = fmaxf(-1.0f, fminf(cosTheta, 1.0f));

    // If vectors are nearly parallel (collinear), fallback to Lerp to avoid division by zero
    if (cosTheta > 0.9995f)
        return Vector2Lerp(start, end, t);

    // Calculate the angle and its sine
    float theta = acosf(cosTheta);
    float sinTheta = sinf(theta);

    // Compute Slerp scale factors for the direction
    float ratioA = sinf((1.0f - t) * theta) / sinTheta;
    float ratioB = sinf(t * theta) / sinTheta;

    // Interpolate the direction
    Vector2 resultDir = {
        (nStart.x * ratioA) + (nEnd.x * ratioB),
        (nStart.y * ratioA) + (nEnd.y * ratioB)
    };

    // Linearly interpolate the magnitudes (lengths) 
    float targetLength = lenStart + (lenEnd - lenStart) * t;

    // Scale the resulting direction by the interpolated magnitude
    Vector2 finalVector = {
        resultDir.x * targetLength,
        resultDir.y * targetLength
    };

    return finalVector;
}



//
// Distance between two points.
//
float Vector2Distance(Vector2 a, Vector2 b)
{
    return Vector2Length(Vector2Subtract(a, b));
}