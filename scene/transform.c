#include "scene.h"



// Sets a transforms local position to a specific Vector3
void Transform_SetLocalPosition(Transform* t, Vector3 position)
{
    if (!t) return;
    t->local_position = position;
    t->is_dirty = true;
}



// Sets a transforms local rotation to specific euler angles
void Transform_SetLocalRotationEuler(Transform* t, Vector3 euler_angles)
{
    if (!t) return;
    t->local_rotation_euler = euler_angles;
    t->local_rotation = QuaternionFromEuler(euler_angles.x, euler_angles.y, euler_angles.z);
    t->is_dirty = true;
}



// Sets a transforms local rotation to specific Quaternion
void Transform_SetLocalRotation(Transform* t, Quaternion rotation)
{
    if (!t) return;
    t->local_rotation = rotation;
    t->local_rotation_euler = QuaternionToEuler(rotation);
    t->is_dirty = true;
}



// Sets a transforms local scale to specific Vector3
void Transform_SetLocalScale(Transform* t, Vector3 scale)
{
    if (!t) return;
    t->local_scale = scale;
    t->is_dirty = true;
}









// Sets the global position, calculating the necessary local offset
void Transform_SetGlobalPosition(Transform* t, Transform* parent_t, Vector3 global_position)
{
    if (!t) return;

    // If no parent, then local is the global
    if (!parent_t) 
    {
        t->local_position = global_position;
    }
    else 
    {
        // If there's a parent, transform the desired global position by the inverse of the parent's matrix.
        Matrix4 parent_inverse = Matrix4Inverse(parent_t->world_matrix);
        t->local_position = Matrix4MultiplyVector3(parent_inverse, global_position);
    }
    
    t->is_dirty = true;
}





// Sets the global rotation
void Transform_SetGlobalRotation(Transform* t, Transform* parent_t, Quaternion global_rotation)
{
    if (!t) return;

    if (!parent_t) 
    {
        t->local_rotation = global_rotation;
    }
    else 
    {
        // To get the local rotation we multiply the inverse of the parent's global rotation by the target global rotation
        Quaternion parent_global_rot = Transform_GetGlobalRotation(parent_t);
        Quaternion parent_inv_rot = QuaternionInverse(parent_global_rot);
        
        t->local_rotation = QuaternionMultiply(parent_inv_rot, global_rotation);
    }

    t->local_rotation_euler = QuaternionToEuler(t->local_rotation);
    t->is_dirty = true;
}





// Sets the global rotation via Euler angles
void Transform_SetGlobalRotationEuler(Transform* t, Transform* parent_t, Vector3 global_euler)
{
    Quaternion global_rot = QuaternionFromEuler(global_euler.x, global_euler.y, global_euler.z);
    Transform_SetGlobalRotation(t, parent_t, global_rot);
}





// Sets the global scale
void Transform_SetGlobalScale(Transform* t, Transform* parent_t, Vector3 global_scale)
{
    if (!t) return;

    if (!parent_t) 
    {
        t->local_scale = global_scale;
    }
    else 
    {
        // Local scale is the target global scale divided by the parent's global scale.
        Vector3 parent_global_scale = Transform_GetGlobalScale(parent_t);
        
        t->local_scale = (Vector3){
            global_scale.x / parent_global_scale.x,
            global_scale.y / parent_global_scale.y,
            global_scale.z / parent_global_scale.z
        };
    }
    
    t->is_dirty = true;
}










// Moves a transforms position by a specified Vector3
void Transform_Translate(Transform* t, Vector3 translation)
{
    if (!t) return;
    t->local_position.x += translation.x;
    t->local_position.y += translation.y;
    t->local_position.z += translation.z;
    t->is_dirty = true;
}



// Moves a transforms rotation by a specified euler angles
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





// Get a transform local position
Vector3 Transform_GetLocalPosition(Transform* t)
{
    if (t)
        return t->local_position;
    else
        return (Vector3){0, 0, 0};
}



// Get a transforms global position
Vector3 Transform_GetGlobalPosition(Transform* t)
{
    if (!t) return (Vector3){0, 0, 0};

    // Extract the global position straight from world matrix
    // (Assuming column-major Matrix4 structure)
    return (Vector3){
        t->world_matrix.m12,
        t->world_matrix.m13,
        t->world_matrix.m14
    };
}



// Get a transforms global scale
Vector3 Transform_GetGlobalScale(Transform* t)
{
    if (!t) return (Vector3){1, 1, 1};

    // Scale is the magnitude (length) of the first 3 column vectors in the matrix
    Vector3 scale;
    scale.x = sqrtf(t->world_matrix.m0 * t->world_matrix.m0 + t->world_matrix.m1 * t->world_matrix.m1 + t->world_matrix.m2 * t->world_matrix.m2);
    scale.y = sqrtf(t->world_matrix.m4 * t->world_matrix.m4 + t->world_matrix.m5 * t->world_matrix.m5 + t->world_matrix.m6 * t->world_matrix.m6);
    scale.z = sqrtf(t->world_matrix.m8 * t->world_matrix.m8 + t->world_matrix.m9 * t->world_matrix.m9 + t->world_matrix.m10 * t->world_matrix.m10);
    
    return scale;
}



// Get a transforms global rotation
Quaternion Transform_GetGlobalRotation(Transform* t)
{
    if (!t) return QuaternionIdentity();

    // Convert the 3x3 rotation portion of the world matrix back into a Quaternion.
    return QuaternionFromMatrix(t->world_matrix); 
}










// Returns the normalized global forward direction of a given transform
Vector3 Transform_GetForwardVector(Transform* t)
{
    if (!t) return (Vector3){0, 0, -1};

    // Extract the Z axis column
    Vector3 forward = {
        -t->world_matrix.m8, 
        -t->world_matrix.m9, 
        -t->world_matrix.m10
    };

    // Normalize vector
    float length = sqrtf(forward.x*forward.x + forward.y*forward.y + forward.z*forward.z);

    if (length > 0.0f)
    {
        forward.x /= length;
        forward.y /= length;
        forward.z /= length;
    }

    return forward;
}



// Returns the normalized global right direction of a given transform
Vector3 Transform_GetRightVector(Transform* t)
{
    if (!t) return (Vector3){1, 0, 0};

    // Extract the X axis column
    Vector3 right = {
        t->world_matrix.m0, 
        t->world_matrix.m1, 
        t->world_matrix.m2
    };

    // Normalize vector
    float length = sqrtf(right.x*right.x + right.y*right.y + right.z*right.z);

    if (length > 0.0f)
    {
        right.x /= length;
        right.y /= length;
        right.z /= length;
    }

    return right;
}



// Returns the normalized global up direction of a given transform
Vector3 Transform_GetUpVector(Transform* t)
{
    if (!t) return (Vector3){0, 1, 0};

    // Extract the Y axis column
    Vector3 up = {
        t->world_matrix.m4, 
        t->world_matrix.m5, 
        t->world_matrix.m6
    };

    // Normalize vector
    float length = sqrtf(up.x*up.x + up.y*up.y + up.z*up.z);
    
    if (length > 0.0f)
    {
        up.x /= length;
        up.y /= length;
        up.z /= length;
    }

    return up;
}