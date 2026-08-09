#ifndef FRUSTUM_H
#define FRUSTUM_H


#include "../core/math_core.h"
#include "../core/mesh_core.h"


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





// ----- Frustum functions -----

// Extracts the 6 planes from a view-projection matrix
Frustum Frustum_ExtractFromMatrix(Matrix4 vp);
// Checks if a sphere is inside the frustum
bool Frustum_ContainsAABB(Frustum* frustum, AABB local_aabb, Matrix4 world_matrix);

// Builds a texel-snapped light-space matrix that fully contains the eight frustum corners of one cascade slice
void ComputeCascadeLightMatrix(const Vector3 corners[8], Vector3 light_dir, Vector3 up, float light_distance, Matrix4* out_light_space, float* out_texel_world_size);
// Builds the eight world-space corners of a camera frustum slice.
void BuildFrustumSliceCorners(Vector3 cam_pos, Vector3 cam_fwd, Vector3 cam_right, Vector3 cam_up, float aspect, float tan_half, float split_near, float split_far, Vector3 corners[8]);





#endif // FRUSTUM_H