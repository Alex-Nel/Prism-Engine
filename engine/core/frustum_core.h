#ifndef FRUSTUM_CORE_H
#define FRUSTUM_CORE_H


#include <stdbool.h>

#include "math_core.h"
#include "mesh_core.h"





// A Frustum Plane
typedef struct FrustumPlane
{
    Vector3 normal;
    float distance;
} FrustumPlane;



// A whole frustum made of 6 planes
typedef struct Frustum
{
    FrustumPlane planes[6];
} Frustum;





Frustum Frustum_ExtractFromMatrix(Matrix4 vp);
bool Frustum_ContainsAABB(Frustum* frustum, AABB local_aabb, Matrix4 world_matrix);
void Frustum_BuildSliceCorners(Vector3 cam_pos, Vector3 cam_fwd, Vector3 cam_right, Vector3 cam_up, float aspect, float tan_half, float split_near, float split_far, Vector3 corners[8]);





#endif