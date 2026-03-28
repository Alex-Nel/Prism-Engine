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