#ifndef VECTOR4_H
#define VECTOR4_H





// A 3D vector
typedef struct Vector4
{
    float x;
    float y;
    float z;
    float w;
} Vector4;



// --- Vector4 operations ---

Vector4 Vector4Add(Vector4 a, Vector4 b);
Vector4 Vector4Subtract(Vector4 a, Vector4 b);
Vector4 Vector4Multiply(Vector4 a, Vector4 b);
Vector4 Vector4Divide(Vector4 v, float s);
Vector4 Vector4Normalize(Vector4 v);
float Vector4Dot(Vector4 a, Vector4 b);
Vector4 Vector4Scale(Vector4 v, float s);
Vector4 Vector4Negate(Vector4 v);
float Vector4Length(Vector4 v);
Vector4 Vector4Lerp(Vector4 start, Vector4 end, float t);
float Vector4Distance(Vector4 a, Vector4 b);





#endif