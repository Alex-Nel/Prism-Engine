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
Vector2 Vector2Normalize(Vector2 v);
float Vector2Dot(Vector2 a, Vector2 b);
Vector2 Vector2Scale(Vector2 v, float s);
Vector2 Vector2Lerp(Vector2 start, Vector2 end, float t);


#endif