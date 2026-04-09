#include "Quaternion.h"
#include <math.h>




///////////////////////////////
// Functions for Quaternions //
///////////////////////////////

// Normalize a quaternion to it's basic form
Quaternion QuaternionNormalize(Quaternion q)
{
    float len = sqrtf(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    
    if (len == 0)
        return (Quaternion){0, 0, 0, 1};
    
    float inv = 1.0f / len;

    Quaternion qr = {q.x*inv, q.y*inv, q.z*inv, q.w*inv};

    return qr;
}



// Get the quaternion representation of a specific set of axis angles
Quaternion QuaternionFromAxisAngle(float ax, float ay, float az, float angle)
{
    float half = angle * 0.5f;
    float s = sinf(half);

    Quaternion q = {ax *s, ay * s, az *s, cosf(half)};

    return QuaternionNormalize(q);
}



// Multiplying two quaternions
Quaternion QuaternionMultiply(Quaternion a, Quaternion b)
{
    Quaternion q = {
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    };

    return q;
}



// Rotating an exact vector by a quaternion
Vector3 RotateVectorByQuaternion(Vector3 v, Quaternion q)
{

    // ------- Unoptimized version -------
    // // v' = q * v * q^-1
    // Quaternion vq = {v.x, v.y, v.z, 0};
    // Quaternion qi = {-q.x, -q.y, -q.z, q.w};

    // Quaternion r = QuaternionMultiply(QuaternionMultiply(q, vq), qi);
    // Vector3 rv = {r.x, r.y, r.z};
    
    // return rv;


    // ------- Optimized version -------
    // Extract the vector part of the quaternion
    Vector3 qv = {q.x, q.y, q.z};
    
    // t = 2 * cross(q.xyz, v)
    Vector3 t = Vector3Scale(Vector3Cross(qv, v), 2.0f);
    
    // v' = v + q.w * t + cross(q.xyz, t)
    Vector3 step1 = Vector3Scale(t, q.w);
    Vector3 step2 = Vector3Cross(qv, t);
    
    return Vector3Add(v, Vector3Add(step1, step2));
}



//
// Get the euler pitch from a given quaternion
//
float GetPitchFromQuaternion(Quaternion q)
{
    // Standard formula: pitch = arcsin(2*(w*y - z*x))
    float v = 2.0f * (q.w*q.y - q.z*q.x);

    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;

    return asinf(v);
}



//
// Get the euler equivalent Yaw
//
float GetYawFromQuaternion(Quaternion q)
{
    // Formula: yaw = atan2(2*(w*z + x*y), 1 - 2*(y*y + z*z))
    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);

    return atan2f(siny_cosp, cosy_cosp);
}



//
// Get the Roll equivalent from a quaternion
//
float GetRollFromQuaternion(Quaternion q)
{
    // Formula: roll = atan2(2*(w*x + y*z), 1 - 2*(x*x + y*y))
    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    return atan2f(sinr_cosp, cosr_cosp);
}


//
// Gets the euler angles from a quaternion
//
Vector3 QuaternionToEuler(Quaternion q)
{
    Vector3 euler;

    // Pitch (x-axis rotation)
    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    
    // Clamp to handle numerical errors near gimbal lock (90 degrees)
    if (sinp > 1.0f) sinp = 1.0f;
    if (sinp < -1.0f) sinp = -1.0f;
    euler.x = asinf(sinp);

    // Yaw (y-axis rotation)
    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    euler.y = atan2f(siny_cosp, cosy_cosp);

    // Roll (z-axis rotation)
    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    euler.z = atan2f(sinr_cosp, cosr_cosp);

    return euler;
}



//
// Get a quaternion from a set of euler angles
//
Quaternion QuaternionFromEuler(float pitch, float yaw, float roll)
{
    float cp = cosf(pitch * 0.5f);
    float sp = sinf(pitch * 0.5f);
    float cy = cosf(yaw * 0.5f);
    float sy = sinf(yaw * 0.5f);
    float cr = cosf(roll * 0.5f);
    float sr = sinf(roll * 0.5f);

    Quaternion q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = cr * sp * cy + sr * cp * sy; // Pitch applies to the X-axis
    q.y = cr * cp * sy - sr * sp * cy; // Yaw applies to the Y-axis
    q.z = sr * cp * cy - cr * sp * sy; // Roll applies to the Z-axis

    return q;
}



// Get the inverse of a quaternion
Quaternion QuaternionInverse(Quaternion q)
{
    float lenSq = q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z;

    if (lenSq == 0.0f)
    {
        Quaternion qr = {0, 0, 0, 1};
        return qr; // identity fallback
    }

    Quaternion qr = {
       -q.x / lenSq,
       -q.y / lenSq,
       -q.z / lenSq,
        q.w / lenSq
    };

    return qr;
}



//
// Get the identity quaternion
//
Quaternion QuaternionIdentity()
{
    Quaternion q = {0.0f, 0.0f, 0.0f, 1.0f};
    return q;
}