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
    
    m.m0 = 1.0f; m.m5 = 1.0f; m.m10 = 1.0f; m.m15 = 1.0f;

    return m;
}



//
// Multiply two 4x4 Matrices.
//
Matrix4 Matrix4Multiply(Matrix4 A, Matrix4 B)
{
    Matrix4 out;

    // Column 1
    out.m0  = A.m0*B.m0  + A.m4*B.m1  + A.m8*B.m2  + A.m12*B.m3;
    out.m1  = A.m1*B.m0  + A.m5*B.m1  + A.m9*B.m2  + A.m13*B.m3;
    out.m2  = A.m2*B.m0  + A.m6*B.m1  + A.m10*B.m2 + A.m14*B.m3;
    out.m3  = A.m3*B.m0  + A.m7*B.m1  + A.m11*B.m2 + A.m15*B.m3;

    // Column 2
    out.m4  = A.m0*B.m4  + A.m4*B.m5  + A.m8*B.m6  + A.m12*B.m7;
    out.m5  = A.m1*B.m4  + A.m5*B.m5  + A.m9*B.m6  + A.m13*B.m7;
    out.m6  = A.m2*B.m4  + A.m6*B.m5  + A.m10*B.m6 + A.m14*B.m7;
    out.m7  = A.m3*B.m4  + A.m7*B.m5  + A.m11*B.m6 + A.m15*B.m7;

    // Column 3
    out.m8  = A.m0*B.m8  + A.m4*B.m9  + A.m8*B.m10 + A.m12*B.m11;
    out.m9  = A.m1*B.m8  + A.m5*B.m9  + A.m9*B.m10 + A.m13*B.m11;
    out.m10 = A.m2*B.m8  + A.m6*B.m9  + A.m10*B.m10+ A.m14*B.m11;
    out.m11 = A.m3*B.m8  + A.m7*B.m9  + A.m11*B.m10+ A.m15*B.m11;

    // Column 4
    out.m12 = A.m0*B.m12 + A.m4*B.m13 + A.m8*B.m14 + A.m12*B.m15;
    out.m13 = A.m1*B.m12 + A.m5*B.m13 + A.m9*B.m14 + A.m13*B.m15;
    out.m14 = A.m2*B.m12 + A.m6*B.m13 + A.m10*B.m14+ A.m14*B.m15;
    out.m15 = A.m3*B.m12 + A.m7*B.m13 + A.m11*B.m14+ A.m15*B.m15;

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
    
    m.m0 = 1.0f / (aspect * t);
    m.m5 = 1.0f / (t);
    m.m10 = -(farZ + nearZ) / (farZ - nearZ);
    m.m11 = -1.0f;
    m.m14 = -(2.0f * farZ * nearZ) / (farZ - nearZ);
    m.m15 = 0.0f;
    
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

    m.m0 = 1.0f - 2.0f * (yy + zz);
    m.m1 = 2.0f * (xy + wz);
    m.m2 = 2.0f * (xz - wy);

    m.m4 = 2.0f * (xy - wz);
    m.m5 = 1.0f - 2.0f * (xx + zz);
    m.m6 = 2.0f * (yz + wx);

    m.m8 = 2.0f * (xz + wy);
    m.m9 = 2.0f * (yz - wx);
    m.m10 = 1.0f - 2.0f * (xx + yy);

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

    r.m1  = m.m4;  r.m4  = m.m1;
    r.m2  = m.m8;  r.m8  = m.m2;
    r.m3  = m.m12; r.m12 = m.m3;

    r.m6  = m.m9;  r.m9  = m.m6;
    r.m7  = m.m13; r.m13 = m.m7;

    r.m11 = m.m14; r.m14 = m.m11;

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

    m.m0 = s.x;  m.m4 = s.y;  m.m8  = s.z;
    m.m1 = u.x;  m.m5 = u.y;  m.m9  = u.z;
    m.m2 = -f.x; m.m6 = -f.y; m.m10 = -f.z;

    m.m12 = -Vector3Dot(s, eye);
    m.m13 = -Vector3Dot(u, eye);
    m.m14 =  Vector3Dot(f, eye);

    return m;
}



//
// The ortho matrix.
//
Matrix4 Matrix4Ortho(float left, float right, float bottom, float top, float nearZ, float farZ)
{
    Matrix4 m = Matrix4Identity();

    m.m0  = 2.0f / (right - left);
    m.m5  = 2.0f / (top - bottom);
    m.m10 = -2.0f / (farZ - nearZ);

    m.m12 = -(right + left) / (right - left);
    m.m13 = -(top + bottom) / (top - bottom);
    m.m14 = -(farZ + nearZ) / (farZ - nearZ);

    return m;
}



//
// Getting the inverse of a matrix.
//
Matrix4 Matrix4Inverse(Matrix4 m)
{
    Matrix4 inv;
    float det;

    inv.m0 = m.m5  * m.m10 * m.m15 - 
             m.m5  * m.m11 * m.m14 - 
             m.m9  * m.m6  * m.m15 + 
             m.m9  * m.m7  * m.m14 +
             m.m13 * m.m6  * m.m11 - 
             m.m13 * m.m7  * m.m10;

    inv.m4 = -m.m4  * m.m10 * m.m15 + 
              m.m4  * m.m11 * m.m14 + 
              m.m8  * m.m6  * m.m15 - 
              m.m8  * m.m7  * m.m14 - 
              m.m12 * m.m6  * m.m11 + 
              m.m12 * m.m7  * m.m10;

    inv.m8 = m.m4  * m.m9 * m.m15 - 
             m.m4  * m.m11 * m.m13 - 
             m.m8  * m.m5 * m.m15 + 
             m.m8  * m.m7 * m.m13 + 
             m.m12 * m.m5 * m.m11 - 
             m.m12 * m.m7 * m.m9;

    inv.m12 = -m.m4  * m.m9 * m.m14 + 
               m.m4  * m.m10 * m.m13 +
               m.m8  * m.m5 * m.m14 - 
               m.m8  * m.m6 * m.m13 - 
               m.m12 * m.m5 * m.m10 + 
               m.m12 * m.m6 * m.m9;

    inv.m1 = -m.m1  * m.m10 * m.m15 + 
              m.m1  * m.m11 * m.m14 + 
              m.m9  * m.m2 * m.m15 - 
              m.m9  * m.m3 * m.m14 - 
              m.m13 * m.m2 * m.m11 + 
              m.m13 * m.m3 * m.m10;

    inv.m5 = m.m0  * m.m10 * m.m15 - 
             m.m0  * m.m11 * m.m14 - 
             m.m8  * m.m2 * m.m15 + 
             m.m8  * m.m3 * m.m14 + 
             m.m12 * m.m2 * m.m11 - 
             m.m12 * m.m3 * m.m10;

    inv.m9 = -m.m0  * m.m9 * m.m15 + 
              m.m0  * m.m11 * m.m13 + 
              m.m8  * m.m1 * m.m15 - 
              m.m8  * m.m3 * m.m13 - 
              m.m12 * m.m1 * m.m11 + 
              m.m12 * m.m3 * m.m9;

    inv.m13 = m.m0  * m.m9 * m.m14 - 
              m.m0  * m.m10 * m.m13 - 
              m.m8  * m.m1 * m.m14 + 
              m.m8  * m.m2 * m.m13 + 
              m.m12 * m.m1 * m.m10 - 
              m.m12 * m.m2 * m.m9;

    inv.m2 = m.m1  * m.m6 * m.m15 - 
             m.m1  * m.m7 * m.m14 - 
             m.m5  * m.m2 * m.m15 + 
             m.m5  * m.m3 * m.m14 + 
             m.m13 * m.m2 * m.m7 - 
             m.m13 * m.m3 * m.m6;

    inv.m6 = -m.m0  * m.m6 * m.m15 + 
              m.m0  * m.m7 * m.m14 + 
              m.m4  * m.m2 * m.m15 - 
              m.m4  * m.m3 * m.m14 - 
              m.m12 * m.m2 * m.m7 + 
              m.m12 * m.m3 * m.m6;

    inv.m10 = m.m0  * m.m5 * m.m15 - 
              m.m0  * m.m7 * m.m13 - 
              m.m4  * m.m1 * m.m15 + 
              m.m4  * m.m3 * m.m13 + 
              m.m12 * m.m1 * m.m7 - 
              m.m12 * m.m3 * m.m5;

    inv.m14 = -m.m0  * m.m5 * m.m14 + 
               m.m0  * m.m6 * m.m13 + 
               m.m4  * m.m1 * m.m14 - 
               m.m4  * m.m2 * m.m13 - 
               m.m12 * m.m1 * m.m6 + 
               m.m12 * m.m2 * m.m5;

    inv.m3 = -m.m1 * m.m6 * m.m11 + 
              m.m1 * m.m7 * m.m10 + 
              m.m5 * m.m2 * m.m11 - 
              m.m5 * m.m3 * m.m10 - 
              m.m9 * m.m2 * m.m7 + 
              m.m9 * m.m3 * m.m6;

    inv.m7 = m.m0 * m.m6 * m.m11 - 
             m.m0 * m.m7 * m.m10 - 
             m.m4 * m.m2 * m.m11 + 
             m.m4 * m.m3 * m.m10 + 
             m.m8 * m.m2 * m.m7 - 
             m.m8 * m.m3 * m.m6;

    inv.m11 = -m.m0 * m.m5 * m.m11 + 
               m.m0 * m.m7 * m.m9 + 
               m.m4 * m.m1 * m.m11 - 
               m.m4 * m.m3 * m.m9 - 
               m.m8 * m.m1 * m.m7 + 
               m.m8 * m.m3 * m.m5;

    inv.m15 = m.m0 * m.m5 * m.m10 - 
              m.m0 * m.m6 * m.m9 - 
              m.m4 * m.m1 * m.m10 + 
              m.m4 * m.m2 * m.m9 + 
              m.m8 * m.m1 * m.m6 - 
              m.m8 * m.m2 * m.m5;

    det = m.m0 * inv.m0 + m.m1 * inv.m4 + m.m2 * inv.m8 + m.m3 * inv.m12;

    if (det == 0) {
        // Return identity if the matrix can't be inverted
        return (Matrix4){ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    }

    det = 1.0f / det;

    for (int i = 0; i < 16; i++) {
        ((float*)&inv)[i] *= det;
    }

    return inv;
}
// Matrix4 Matrix4Inverse(Matrix4 m)
// {
//     // Extract rotation (upper 3x3)
//     Matrix4 r = m;

//     // Transpose rotation
//     r = Matrix4Transpose(r);

//     // Extract translation
//     Vector3 t = {m.m12, m.m13, m.m14};

//     // New translation = -(R^T * t)
//     Vector3 nt = {
//         -(r.m0 * t.x + r.m4 * t.y + r.m8  * t.z),
//         -(r.m1 * t.x + r.m5 * t.y + r.m9  * t.z),
//         -(r.m2 * t.x + r.m6 * t.y + r.m10 * t.z)
//     };

//     r.m12 = nt.x;
//     r.m13 = nt.y;
//     r.m14 = nt.z;

//     return r;
// }



//
// Translating a Matrix by a certain value.
//
Matrix4 Matrix4Translate(Vector3 t)
{
    Matrix4 m = Matrix4Identity();
    m.m12 = t.x;
    m.m13 = t.y;
    m.m14 = t.z;

    return m;
}



//
// Scaling a matrix by a scalar vector.
//
Matrix4 Matrix4Scale(Vector3 s)
{
    Matrix4 m = Matrix4Identity();
    m.m0 = s.x;
    m.m5 = s.y;
    m.m10 = s.z;

    return m;
}



//
// Rotate a matrix by an angle.
//
Matrix4 Matrix4RotateX(float r)
{
    Matrix4 m = Matrix4Identity();
    float c = cosf(r), s = sinf(r);
    m.m5 = c;  m.m9  = -s;
    m.m6 = s;  m.m10 = c;

    return m;
}



//
// Multiplies a matrix by a vector3 to return a vector3
//
Vector3 Matrix4MultiplyVector3(Matrix4 m, Vector3 v)
{
    Vector3 out;

    out.x = m.m0 * v.x + m.m4 * v.y + m.m8  * v.z + m.m12;
    out.y = m.m1 * v.x + m.m5 * v.y + m.m9  * v.z + m.m13;
    out.z = m.m2 * v.x + m.m6 * v.y + m.m10 * v.z + m.m14;

    return out;
}



//
// Get a quaternion from a given matrix
//
Quaternion QuaternionFromMatrix(Matrix4 m)
{
    Quaternion q;
    
    // 1. Extract the scale squared
    float sx = m.m0 * m.m0 + m.m1 * m.m1 + m.m2 * m.m2;
    float sy = m.m4 * m.m4 + m.m5 * m.m5 + m.m6 * m.m6;
    float sz = m.m8 * m.m8 + m.m9 * m.m9 + m.m10 * m.m10;

    // 2. Normalize the matrix axes to strip the scale out
    // (We add a tiny epsilon to prevent dividing by zero if scale is 0)
    float inv_sx = 1.0f / sqrtf(sx + 0.00001f);
    float inv_sy = 1.0f / sqrtf(sy + 0.00001f);
    float inv_sz = 1.0f / sqrtf(sz + 0.00001f);

    float m00 = m.m0 * inv_sx, m01 = m.m4 * inv_sy, m02 = m.m8 * inv_sz;
    float m10 = m.m1 * inv_sx, m11 = m.m5 * inv_sy, m12 = m.m9 * inv_sz;
    float m20 = m.m2 * inv_sx, m21 = m.m6 * inv_sy, m22 = m.m10 * inv_sz;

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