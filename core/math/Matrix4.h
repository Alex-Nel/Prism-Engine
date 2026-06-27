#ifndef MATRIX4_H
#define MATRIX4_H

#include "Vector3.h"
#include "Vector4.h"
#include "Quaternion.h"

// A 4x4 matrix
typedef struct Matrix4
{
    float m00, m10, m20, m30;
    float m01, m11, m21, m31;
    float m02, m12, m22, m32;
    float m03, m13, m23, m33;
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
Vector3 Matrix4MultiplyVector3(Matrix4 m, Vector3 v);
Vector4 Matrix4MultiplyVector4(Matrix4 m, Vector4 v);
Quaternion QuaternionFromMatrix(Matrix4 m);
void Matrix4ToArray(Matrix4 m, float out[16]);



#endif