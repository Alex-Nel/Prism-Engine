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
// Linearly interpolate a vector from one point to anther given a time period.
//
Vector3 Vector3Lerp(Vector3 start, Vector3 end, float t) {
    Vector3 v = {
        start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t,
        start.z + (end.z - start.z) * t
    };

    return v;
}