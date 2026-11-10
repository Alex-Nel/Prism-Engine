#include "frustum_core.h"
#include <math.h>



// Normalizes a frustum plane
static void NormalizePlane(FrustumPlane* p)
{
    float length = sqrtf(p->normal.x * p->normal.x + p->normal.y * p->normal.y + p->normal.z * p->normal.z);
    if (length > 0.0001f)
    {
        p->normal.x /= length;
        p->normal.y /= length;
        p->normal.z /= length;
        p->distance /= length;
    }
}





// Extracts the 6 planes from a view-projection matrix
Frustum Frustum_ExtractFromMatrix(Matrix4 vp)
{
    Frustum f;
    
    f.planes[0].normal.x = vp.m30 + vp.m00;
    f.planes[0].normal.y = vp.m31 + vp.m01;
    f.planes[0].normal.z = vp.m32 + vp.m02;
    f.planes[0].distance = vp.m33 + vp.m03;
    
    f.planes[1].normal.x = vp.m30 - vp.m00;
    f.planes[1].normal.y = vp.m31 - vp.m01;
    f.planes[1].normal.z = vp.m32 - vp.m02;
    f.planes[1].distance = vp.m33 - vp.m03;
    
    f.planes[2].normal.x = vp.m30 + vp.m10;
    f.planes[2].normal.y = vp.m31 + vp.m11;
    f.planes[2].normal.z = vp.m32 + vp.m12;
    f.planes[2].distance = vp.m33 + vp.m13;
    
    f.planes[3].normal.x = vp.m30 - vp.m10;
    f.planes[3].normal.y = vp.m31 - vp.m11;
    f.planes[3].normal.z = vp.m32 - vp.m12;
    f.planes[3].distance = vp.m33 - vp.m13;
    
    f.planes[4].normal.x = vp.m30 + vp.m20;
    f.planes[4].normal.y = vp.m31 + vp.m21;
    f.planes[4].normal.z = vp.m32 + vp.m22;
    f.planes[4].distance = vp.m33 + vp.m23;
    
    f.planes[5].normal.x = vp.m30 - vp.m20;
    f.planes[5].normal.y = vp.m31 - vp.m21;
    f.planes[5].normal.z = vp.m32 - vp.m22;
    f.planes[5].distance = vp.m33 - vp.m23;
    
    for (int i = 0; i < 6; i++)
        NormalizePlane(&f.planes[i]);
    
    return f;
}










// Checks if a sphere is inside the frustum
bool Frustum_ContainsAABB(Frustum* frustum, AABB local_aabb, Matrix4 world_matrix)
{
    Vector3 center_local = {
        (local_aabb.max.x + local_aabb.min.x) * 0.5f,
        (local_aabb.max.y + local_aabb.min.y) * 0.5f,
        (local_aabb.max.z + local_aabb.min.z) * 0.5f
    };

    Vector3 extents_local = {
        (local_aabb.max.x - local_aabb.min.x) * 0.5f,
        (local_aabb.max.y - local_aabb.min.y) * 0.5f,
        (local_aabb.max.z - local_aabb.min.z) * 0.5f
    };

    Vector3 center_world;
    center_world.x = world_matrix.m00 * center_local.x + world_matrix.m01 * center_local.y + world_matrix.m02 * center_local.z + world_matrix.m03;
    center_world.y = world_matrix.m10 * center_local.x + world_matrix.m11 * center_local.y + world_matrix.m12 * center_local.z + world_matrix.m13;
    center_world.z = world_matrix.m20 * center_local.x + world_matrix.m21 * center_local.y + world_matrix.m22 * center_local.z + world_matrix.m23;
    
    Vector3 extents_world;
    extents_world.x = fabsf(world_matrix.m00) * extents_local.x + fabsf(world_matrix.m01) * extents_local.y + fabsf(world_matrix.m02) * extents_local.z;
    extents_world.y = fabsf(world_matrix.m10) * extents_local.x + fabsf(world_matrix.m11) * extents_local.y + fabsf(world_matrix.m12) * extents_local.z;
    extents_world.z = fabsf(world_matrix.m20) * extents_local.x + fabsf(world_matrix.m21) * extents_local.y + fabsf(world_matrix.m22) * extents_local.z;
    
    for (int i = 0; i < 6; i++)
    {
        FrustumPlane plane = frustum->planes[i];
        
        float r = extents_world.x * fabsf(plane.normal.x) +
                  extents_world.y * fabsf(plane.normal.y) +
                  extents_world.z * fabsf(plane.normal.z);
        float d = plane.normal.x * center_world.x +
                  plane.normal.y * center_world.y +
                  plane.normal.z * center_world.z +
                  plane.distance;
        
        if (d < -r)
            return false;
    }

    return true;
}










// Builds the eight world-space corners of a camera frustum slice.
void Frustum_BuildSliceCorners(Vector3 cam_pos, Vector3 cam_fwd, Vector3 cam_right, Vector3 cam_up, float aspect, float tan_half, float split_near, float split_far, Vector3 corners[8])
{
    float near_h = split_near * tan_half;
    float near_w = near_h * aspect;
    float far_h = split_far * tan_half;
    float far_w = far_h * aspect;
    Vector3 near_center = {
        cam_pos.x + cam_fwd.x * split_near,
        cam_pos.y + cam_fwd.y * split_near,
        cam_pos.z + cam_fwd.z * split_near
    };
    Vector3 far_center = {
        cam_pos.x + cam_fwd.x * split_far,
        cam_pos.y + cam_fwd.y * split_far,
        cam_pos.z + cam_fwd.z * split_far
    };
    corners[0] = (Vector3){ near_center.x - cam_right.x * near_w - cam_up.x * near_h,
                            near_center.y - cam_right.y * near_w - cam_up.y * near_h,
                            near_center.z - cam_right.z * near_w - cam_up.z * near_h };
    corners[1] = (Vector3){ near_center.x + cam_right.x * near_w - cam_up.x * near_h,
                            near_center.y + cam_right.y * near_w - cam_up.y * near_h,
                            near_center.z + cam_right.z * near_w - cam_up.z * near_h };
    corners[2] = (Vector3){ near_center.x + cam_right.x * near_w + cam_up.x * near_h,
                            near_center.y + cam_right.y * near_w + cam_up.y * near_h,
                            near_center.z + cam_right.z * near_w + cam_up.z * near_h };
    corners[3] = (Vector3){ near_center.x - cam_right.x * near_w + cam_up.x * near_h,
                            near_center.y - cam_right.y * near_w + cam_up.y * near_h,
                            near_center.z - cam_right.z * near_w + cam_up.z * near_h };
    corners[4] = (Vector3){ far_center.x - cam_right.x * far_w - cam_up.x * far_h,
                            far_center.y - cam_right.y * far_w - cam_up.y * far_h,
                            far_center.z - cam_right.z * far_w - cam_up.z * far_h };
    corners[5] = (Vector3){ far_center.x + cam_right.x * far_w - cam_up.x * far_h,
                            far_center.y + cam_right.y * far_w - cam_up.y * far_h,
                            far_center.z + cam_right.z * far_w - cam_up.z * far_h };
    corners[6] = (Vector3){ far_center.x + cam_right.x * far_w + cam_up.x * far_h,
                            far_center.y + cam_right.y * far_w + cam_up.y * far_h,
                            far_center.z + cam_right.z * far_w + cam_up.z * far_h };
    corners[7] = (Vector3){ far_center.x - cam_right.x * far_w + cam_up.x * far_h,
                            far_center.y - cam_right.y * far_w + cam_up.y * far_h,
                            far_center.z - cam_right.z * far_w + cam_up.z * far_h };
}