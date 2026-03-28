#ifndef VECTOR3_H
#define VECTOR3_H

// A 3D vector
typedef struct Vector3
{
    float x;
    float y;
    float z;
} Vector3;



// Vector3 operations
Vector3 Vector3Add(Vector3 a, Vector3 b);
Vector3 Vector3Subtract(Vector3 a, Vector3 b);
Vector3 Vector3Normalize(Vector3 v);
Vector3 Vector3Cross(Vector3 a, Vector3 b);
float Vector3Dot(Vector3 a, Vector3 b);
Vector3 Vector3Scale(Vector3 v, float s);
Vector3 Vector3Lerp(Vector3 start, Vector3 end, float t);


#endif