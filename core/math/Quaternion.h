#ifndef QUATERNION_H
#define QUATERNION_H

#include "Vector3.h"



// Used to represent rotation
typedef struct Quaternion
{
    float x;
    float y;
    float z;
    float w;
} Quaternion;




// Quaternion operations
Quaternion QuaternionNormalize(Quaternion q);
Quaternion QuaternionFromAxisAngle(float ax, float ay, float az, float angle);
Quaternion QuaternionMultiply(Quaternion a, Quaternion b);
Vector3 RotateVectorByQuaternion(Vector3 v, Quaternion q);
float GetPitchFromQuaternion(Quaternion q);
float GetYawFromQuaternion(Quaternion q);
float GetRollFromQuaternion(Quaternion q);
Quaternion QuaternionFromEuler(float pitch, float yaw, float roll);
Quaternion QuaternionInverse(Quaternion q);
Quaternion QuaternionIdentity();





#endif