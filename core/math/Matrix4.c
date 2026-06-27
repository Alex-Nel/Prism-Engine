#include "Matrix4.h"
#include <math.h>
#include <string.h>


////////////////////////////////
// Function for for Matrix4's //
////////////////////////////////


//
// Get the identity matrix.
//
Matrix4 Matrix4Identity()
{
    Matrix4 m = {0};
    
    m.m00 = 1.0f; m.m11 = 1.0f; m.m22 = 1.0f; m.m33 = 1.0f;

    return m;
}



//
// Multiply two 4x4 Matrices.
//
Matrix4 Matrix4Multiply(Matrix4 A, Matrix4 B)
{
    Matrix4 out;

    // Column 0
    out.m00 = A.m00*B.m00 + A.m01*B.m10 + A.m02*B.m20 + A.m03*B.m30;
    out.m10 = A.m10*B.m00 + A.m11*B.m10 + A.m12*B.m20 + A.m13*B.m30;
    out.m20 = A.m20*B.m00 + A.m21*B.m10 + A.m22*B.m20 + A.m23*B.m30;
    out.m30 = A.m30*B.m00 + A.m31*B.m10 + A.m32*B.m20 + A.m33*B.m30;

    // Column 1
    out.m01 = A.m00*B.m01 + A.m01*B.m11 + A.m02*B.m21 + A.m03*B.m31;
    out.m11 = A.m10*B.m01 + A.m11*B.m11 + A.m12*B.m21 + A.m13*B.m31;
    out.m21 = A.m20*B.m01 + A.m21*B.m11 + A.m22*B.m21 + A.m23*B.m31;
    out.m31 = A.m30*B.m01 + A.m31*B.m11 + A.m32*B.m21 + A.m33*B.m31;

    // Column 2
    out.m02 = A.m00*B.m02 + A.m01*B.m12 + A.m02*B.m22 + A.m03*B.m32;
    out.m12 = A.m10*B.m02 + A.m11*B.m12 + A.m12*B.m22 + A.m13*B.m32;
    out.m22 = A.m20*B.m02 + A.m21*B.m12 + A.m22*B.m22 + A.m23*B.m32;
    out.m32 = A.m30*B.m02 + A.m31*B.m12 + A.m32*B.m22 + A.m33*B.m32;

    // Column 3
    out.m03 = A.m00*B.m03 + A.m01*B.m13 + A.m02*B.m23 + A.m03*B.m33;
    out.m13 = A.m10*B.m03 + A.m11*B.m13 + A.m12*B.m23 + A.m13*B.m33;
    out.m23 = A.m20*B.m03 + A.m21*B.m13 + A.m22*B.m23 + A.m23*B.m33;
    out.m33 = A.m30*B.m03 + A.m31*B.m13 + A.m32*B.m23 + A.m33*B.m33;

    return out;
}



//
// Function to get the Perspective Projection Matrix.
//
Matrix4 Matrix4Perspective(float fovRadians, float aspect, float nearZ, float farZ)
{
    Matrix4 m = Matrix4Identity();
    // float tanHalfFov = tanf(fovRadians / 2.0f);
    float t = tanf(fovRadians * 0.5f);
    
    m.m00 = 1.0f / (aspect * t);
    m.m11 = 1.0f / (t);
    m.m22 = -(farZ + nearZ) / (farZ - nearZ);
    m.m32 = -1.0f;
    m.m23 = -(2.0f * farZ * nearZ) / (farZ - nearZ);
    m.m33 = 0.0f;
    
    return m;
}



//
// Quaternion to Rotation Matrix.
//
Matrix4 Matrix4FromQuaternion(Quaternion q)
{
    Matrix4 m = Matrix4Identity();
    
    float xx = q.x * q.x; float yy = q.y * q.y; float zz = q.z * q.z;
    float xy = q.x * q.y; float xz = q.x * q.z; float yz = q.y * q.z;
    float wx = q.w * q.x; float wy = q.w * q.y; float wz = q.w * q.z;

    m.m00 = 1.0f - 2.0f * (yy + zz);
    m.m10 = 2.0f * (xy + wz);
    m.m20 = 2.0f * (xz - wy);

    m.m01 = 2.0f * (xy - wz);
    m.m11 = 1.0f - 2.0f * (xx + zz);
    m.m21 = 2.0f * (yz + wx);

    m.m02 = 2.0f * (xz + wy);
    m.m12 = 2.0f * (yz - wx);
    m.m22 = 1.0f - 2.0f * (xx + yy);

    return m;
}



//
// Creates the World Matrix for an Entity's Transform component
//
Matrix4 Matrix4CreateTransform(Vector3 position, Quaternion rotation, Vector3 scale)
{
    Matrix4 t = Matrix4Translate(position);
    Matrix4 r = Matrix4FromQuaternion(rotation);
    Matrix4 s = Matrix4Scale(scale);

    // TRS = Translate * (Rotate * Scale)
    // We multiply Right-to-Left in column-major systems
    Matrix4 rot_scale = Matrix4Multiply(r, s);
    return Matrix4Multiply(t, rot_scale);
}



//
// Creates the View Matrix for the Camera component.
//
Matrix4 Matrix4CreateView(Vector3 position, Quaternion rotation)
{   
    // Invert the position
    Matrix4 inv_trans = Matrix4Translate((Vector3){-position.x, -position.y, -position.z});
    
    // Invert the rotation
    Matrix4 rot = Matrix4FromQuaternion(rotation);
    Matrix4 inv_rot = Matrix4Transpose(rot); 
    
    return Matrix4Multiply(inv_rot, inv_trans); 
}



//
// Transpose of a Matrix.
//
Matrix4 Matrix4Transpose(Matrix4 m)
{
    Matrix4 r = m;

    r.m10 = m.m01; r.m01 = m.m10;
    r.m20 = m.m02; r.m02 = m.m20;
    r.m30 = m.m03; r.m03 = m.m30;

    r.m21 = m.m12; r.m12 = m.m21;
    r.m31 = m.m13; r.m13 = m.m31;

    r.m32 = m.m23; r.m23 = m.m32;

    return r;
}



//
// Get the resulting Matrix when looking at a target.
//
Matrix4 Matrix4LookAt(Vector3 eye, Vector3 target, Vector3 up)
{
    Vector3 f = Vector3Normalize(Vector3Subtract(target, eye));
    Vector3 s = Vector3Normalize(Vector3Cross(f, up));
    Vector3 u = Vector3Cross(s, f);

    Matrix4 m = Matrix4Identity();

    m.m00 = s.x;  m.m01 = s.y;  m.m02 = s.z;
    m.m10 = u.x;  m.m11 = u.y;  m.m12 = u.z;
    m.m20 = -f.x; m.m21 = -f.y; m.m22 = -f.z;

    m.m03 = -Vector3Dot(s, eye);
    m.m13 = -Vector3Dot(u, eye);
    m.m23 =  Vector3Dot(f, eye);

    return m;
}



//
// The ortho matrix.
//
Matrix4 Matrix4Ortho(float left, float right, float bottom, float top, float nearZ, float farZ)
{
    Matrix4 m = Matrix4Identity();

    m.m00  = 2.0f / (right - left);
    m.m11 = 2.0f / (top - bottom);
    m.m22 = -2.0f / (farZ - nearZ);

    m.m03 = -(right + left) / (right - left);
    m.m13 = -(top + bottom) / (top - bottom);
    m.m23 = -(farZ + nearZ) / (farZ - nearZ);

    return m;
}



//
// Getting the inverse of a matrix.
//
Matrix4 Matrix4Inverse(Matrix4 m)
{
    Matrix4 inv;
    float det;

    inv.m00 = m.m11 * m.m22 * m.m33 - 
              m.m11 * m.m32 * m.m23 - 
              m.m12 * m.m21 * m.m33 + 
              m.m12 * m.m31 * m.m23 +
              m.m13 * m.m21 * m.m32 - 
              m.m13 * m.m31 * m.m22;

    inv.m01 = -m.m01 * m.m22 * m.m33 + 
               m.m01 * m.m32 * m.m23 + 
               m.m02 * m.m21 * m.m33 - 
               m.m02 * m.m31 * m.m23 - 
               m.m03 * m.m21 * m.m32 + 
               m.m03 * m.m31 * m.m22;

    inv.m02 = m.m01 * m.m12 * m.m33 - 
              m.m01 * m.m32 * m.m13 - 
              m.m02 * m.m11 * m.m33 + 
              m.m02 * m.m31 * m.m13 + 
              m.m03 * m.m11 * m.m32 - 
              m.m03 * m.m31 * m.m12;

    inv.m03 = -m.m01 * m.m12 * m.m23 + 
               m.m01 * m.m22 * m.m13 +
               m.m02 * m.m11 * m.m23 - 
               m.m02 * m.m21 * m.m13 - 
               m.m03 * m.m11 * m.m22 + 
               m.m03 * m.m21 * m.m12;

    inv.m10 = -m.m10 * m.m22 * m.m33 + 
               m.m10 * m.m32 * m.m23 + 
               m.m12 * m.m20 * m.m33 - 
               m.m12 * m.m30 * m.m23 - 
               m.m13 * m.m20 * m.m32 + 
               m.m13 * m.m30 * m.m22;

    inv.m11 = m.m00 * m.m22 * m.m33 - 
              m.m00 * m.m32 * m.m23 - 
              m.m02 * m.m20 * m.m33 + 
              m.m02 * m.m30 * m.m23 + 
              m.m03 * m.m20 * m.m32 - 
              m.m03 * m.m30 * m.m22;

    inv.m12 = -m.m00 * m.m12 * m.m33 + 
               m.m00 * m.m32 * m.m13 + 
               m.m02 * m.m10 * m.m33 - 
               m.m02 * m.m30 * m.m13 - 
               m.m03 * m.m10 * m.m32 + 
               m.m03 * m.m30 * m.m12;

    inv.m13 = m.m00 * m.m12 * m.m23 - 
              m.m00 * m.m22 * m.m13 - 
              m.m02 * m.m10 * m.m23 + 
              m.m02 * m.m20 * m.m13 + 
              m.m03 * m.m10 * m.m22 - 
              m.m03 * m.m20 * m.m12;

    inv.m20 = m.m10 * m.m21 * m.m33 - 
              m.m10 * m.m31 * m.m23 - 
              m.m11 * m.m20 * m.m33 + 
              m.m11 * m.m30 * m.m23 + 
              m.m13 * m.m20 * m.m31 - 
              m.m13 * m.m30 * m.m21;

    inv.m21 = -m.m00 * m.m21 * m.m33 + 
               m.m00 * m.m31 * m.m23 + 
               m.m01 * m.m20 * m.m33 - 
               m.m01 * m.m30 * m.m23 - 
               m.m03 * m.m20 * m.m31 + 
               m.m03 * m.m30 * m.m21;

    inv.m22 = m.m00 * m.m11 * m.m33 - 
              m.m00 * m.m31 * m.m13 - 
              m.m01 * m.m10 * m.m33 + 
              m.m01 * m.m30 * m.m13 + 
              m.m03 * m.m10 * m.m31 - 
              m.m03 * m.m30 * m.m11;

    inv.m23 = -m.m00 * m.m11 * m.m23 + 
               m.m00 * m.m21 * m.m13 + 
               m.m01 * m.m10 * m.m23 - 
               m.m01 * m.m20 * m.m13 - 
               m.m03 * m.m10 * m.m21 + 
               m.m03 * m.m20 * m.m11;

    inv.m30 = -m.m10 * m.m21 * m.m32 + 
               m.m10 * m.m31 * m.m22 + 
               m.m11 * m.m20 * m.m32 - 
               m.m11 * m.m30 * m.m22 - 
               m.m12 * m.m20 * m.m31 + 
               m.m12 * m.m30 * m.m21;

    inv.m31 = m.m00 * m.m21 * m.m32 - 
              m.m00 * m.m31 * m.m22 - 
              m.m01 * m.m20 * m.m32 + 
              m.m01 * m.m30 * m.m22 + 
              m.m02 * m.m20 * m.m31 - 
              m.m02 * m.m30 * m.m21;

    inv.m32 = -m.m00 * m.m11 * m.m32 + 
               m.m00 * m.m31 * m.m12 + 
               m.m01 * m.m10 * m.m32 - 
               m.m01 * m.m30 * m.m12 - 
               m.m02 * m.m10 * m.m31 + 
               m.m02 * m.m30 * m.m11;

    inv.m33 = m.m00 * m.m11 * m.m22 - 
              m.m00 * m.m21 * m.m12 - 
              m.m01 * m.m10 * m.m22 + 
              m.m01 * m.m20 * m.m12 + 
              m.m02 * m.m10 * m.m21 - 
              m.m02 * m.m20 * m.m11;

    det = m.m00 * inv.m00 + m.m10 * inv.m01 + m.m20 * inv.m02 + m.m30 * inv.m03;

    if (det == 0)
    {
        // Return identity if the matrix can't be inverted
        return (Matrix4){ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    }

    det = 1.0f / det;

    for (int i = 0; i < 16; i++)
    {
        ((float*)&inv)[i] *= det;
    }

    return inv;
}



//
// Translating a Matrix by a certain value.
//
Matrix4 Matrix4Translate(Vector3 t)
{
    Matrix4 m = Matrix4Identity();
    m.m03 = t.x;
    m.m13 = t.y;
    m.m23 = t.z;

    return m;
}



//
// Scaling a matrix by a scalar vector.
//
Matrix4 Matrix4Scale(Vector3 s)
{
    Matrix4 m = Matrix4Identity();
    m.m00 = s.x;
    m.m11 = s.y;
    m.m22 = s.z;

    return m;
}



//
// Rotate a matrix by an angle.
//
Matrix4 Matrix4RotateX(float r)
{
    Matrix4 m = Matrix4Identity();
    float c = cosf(r), s = sinf(r);
    m.m11 = c;  m.m12 = -s;
    m.m21 = s;  m.m22 = c;

    return m;
}



//
// Multiplies a matrix by a vector3 to return a vector3
//
Vector3 Matrix4MultiplyVector3(Matrix4 m, Vector3 v)
{
    Vector3 out;

    out.x = m.m00 * v.x + m.m01 * v.y + m.m02 * v.z + m.m03;
    out.y = m.m10 * v.x + m.m11 * v.y + m.m12 * v.z + m.m13;
    out.z = m.m20 * v.x + m.m21 * v.y + m.m22 * v.z + m.m23;

    return out;
}



//
// Multiplies a matrix by a vector4 to return a vector4
//
Vector4 Matrix4MultiplyVector4(Matrix4 m, Vector4 v)
{
    Vector4 result;

    result.x = m.m00*v.x + m.m01*v.y + m.m02*v.z + m.m03*v.w;
    result.y = m.m10*v.x + m.m11*v.y + m.m12*v.z + m.m13*v.w;
    result.z = m.m20*v.x + m.m21*v.y + m.m22*v.z + m.m23*v.w;
    result.w = m.m30*v.x + m.m31*v.y + m.m32*v.z + m.m33*v.w;
    
    return result;
}



//
// Get a quaternion from a given matrix
//
Quaternion QuaternionFromMatrix(Matrix4 m)
{
    Quaternion q;
    
    // 1. Extract the scale squared
    float sx = m.m00 * m.m00 + m.m10 * m.m10 + m.m20 * m.m20;
    float sy = m.m01 * m.m01 + m.m11 * m.m11 + m.m21 * m.m21;
    float sz = m.m02 * m.m02 + m.m12 * m.m12 + m.m22 * m.m22;

    // 2. Normalize the matrix axes to strip the scale out
    // (We add a tiny epsilon to prevent dividing by zero if scale is 0)
    float inv_sx = 1.0f / sqrtf(sx + 0.00001f);
    float inv_sy = 1.0f / sqrtf(sy + 0.00001f);
    float inv_sz = 1.0f / sqrtf(sz + 0.00001f);

    float m00 = m.m00 * inv_sx, m01 = m.m01 * inv_sy, m02 = m.m02 * inv_sz;
    float m10 = m.m10 * inv_sx, m11 = m.m11 * inv_sy, m12 = m.m12 * inv_sz;
    float m20 = m.m20 * inv_sx, m21 = m.m21 * inv_sy, m22 = m.m22 * inv_sz;

    // 3. The Shoemake Algorithm
    float trace = m00 + m11 + m22;

    if (trace > 0.0f) 
    {
        float s = 0.5f / sqrtf(trace + 1.0f);
        q.w = 0.25f / s;
        q.x = (m21 - m12) * s;
        q.y = (m02 - m20) * s;
        q.z = (m10 - m01) * s;
    } 
    else 
    {
        if (m00 > m11 && m00 > m22) 
        {
            float s = 2.0f * sqrtf(1.0f + m00 - m11 - m22);
            q.w = (m21 - m12) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        } 
        else if (m11 > m22) 
        {
            float s = 2.0f * sqrtf(1.0f + m11 - m00 - m22);
            q.w = (m02 - m20) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        } 
        else 
        {
            float s = 2.0f * sqrtf(1.0f + m22 - m00 - m11);
            q.w = (m10 - m01) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }
    }

    // Ensure the resulting quaternion is perfectly normalized
    float length = sqrtf(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    q.x /= length; q.y /= length; q.z /= length; q.w /= length;

    return q;
}



//
// Get the array representation of a matrix.
//
void Matrix4ToArray(Matrix4 m, float out[16])
{
    memcpy(out, &m, sizeof(Matrix4));
}