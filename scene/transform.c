#include "scene.h"





void Transform_SetLocalPosition(Transform* t, Vector3 position)
{
    if (!t) return;
    t->local_position = position;
    t->is_dirty = true;
}



void Transform_SetLocalRotationEuler(Transform* t, Vector3 euler_angles)
{
    if (!t) return;
    t->local_rotation_euler = euler_angles;
    t->local_rotation = QuaternionFromEuler(euler_angles.x, euler_angles.y, euler_angles.z);
    t->is_dirty = true;
}



void Transform_SetLocalRotation(Transform* t, Quaternion rotation)
{
    if (!t) return;
    t->local_rotation = rotation;
    // t->local_rotation_euler = QuaternionToEuler(rotation);
    t->is_dirty = true;
}



void Transform_SetLocalScale(Transform* t, Vector3 scale)
{
    if (!t) return;
    t->local_scale = scale;
    t->is_dirty = true;
}







void Transform_Translate(Transform* t, Vector3 translation)
{
    if (!t) return;
    t->local_position.x += translation.x;
    t->local_position.y += translation.y;
    t->local_position.z += translation.z;
    t->is_dirty = true;
}



void Transform_RotateEuler(Transform* t, Vector3 euler_addition)
{
    if (!t) return;
    Vector3 new_euler = {
        t->local_rotation_euler.x + euler_addition.x,
        t->local_rotation_euler.y + euler_addition.y,
        t->local_rotation_euler.z + euler_addition.z
    };
    Transform_SetLocalRotationEuler(t, new_euler);
}






Vector3 Transform_GetLocalPosition(Transform* t)
{
    return t ? t->local_position : (Vector3){0,0,0};
}



Vector3 Transform_GetGlobalPosition(Transform* t)
{
    if (!t) return (Vector3){0,0,0};

    // Extract the global position straight from world matrix!
    // (Assuming column-major Matrix4 structure)
    return (Vector3){
        t->world_matrix.m12,
        t->world_matrix.m13,
        t->world_matrix.m14
    };
}