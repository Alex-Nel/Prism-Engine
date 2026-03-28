#ifndef MATRIX4_H
#define MATRIX4_H

#include "Vector3.h"
#include "Quaternion.h"

// A 4x4 matrix used for openGL
typedef struct Matrix4
{
    float m0, m1, m2,  m3;
    float m4, m5, m6,  m7;
    float m8, m9, m10, m11;
    float m12, m13, m14, m15;
} Matrix4;





// Matrix4 operations
Matrix4 Matrix4Identity();
Matrix4 Matrix4Multiply(Matrix4 A, Matrix4 B);
Matrix4 Matrix4Perspective(float fovRadians, float aspect, float nearZ, float farZ);
Matrix4 Matrix4FromQuaternion(Quaternion q);
Matrix4 Matrix4CreateTransform(Vector3 position, Quaternion rotation, Vector3 scale);
Matrix4 Matrix4CreateView(Vector3 position, Quaternion rotation);
Matrix4 Matrix4Transpose(Matrix4 m);
Matrix4 Matrix4LookAt(Vector3 eye, Vector3 target, Vector3 up);
Matrix4 Matrix4Ortho(float left, float right, float bottom, float top, float nearZ, float farZ);
Matrix4 Matrix4Inverse(Matrix4 m);
Matrix4 Matrix4Translate(Vector3 t);
Matrix4 Matrix4Scale(Vector3 s);
Matrix4 Matrix4RotateX(float r);
void Matrix4ToArray(Matrix4 m, float out[16]);



#endif