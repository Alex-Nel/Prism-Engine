#ifndef VECTOR2_H
#define VECTOR2_H



// A 2D vector
typedef struct Vector2
{
    float x;
    float y;
} Vector2;



// Vector2 operations
Vector2 Vector2Add(Vector2 a, Vector2 b);
Vector2 Vector2Subtract(Vector2 a, Vector2 b);
float Vector2Cross(Vector2 a, Vector2 b);
Vector2 Vector2Multiply(Vector2 a, Vector2 b);
Vector2 Vector2Divide(Vector2 v, float s);
Vector2 Vector2Normalize(Vector2 v);
float Vector2Dot(Vector2 a, Vector2 b);
Vector2 Vector2Scale(Vector2 v, float s);
Vector2 Vector2Negate(Vector2 v);
float Vector2Length(Vector2 v);
Vector2 Vector2Lerp(Vector2 start, Vector2 end, float t);
Vector2 Vector2Slerp(Vector2 start, Vector2 end, float t);
float Vector2Distance(Vector2 a, Vector2 b);


#endif