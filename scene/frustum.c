#include "frustum.h"



// Normalizes the plane equation so the normal has a length of 1
static inline void NormalizePlane(FrustumPlane* p) 
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

    // Left Plane (Row 3 + Row 0)
    f.planes[0].normal.x = vp.m30 + vp.m00;
    f.planes[0].normal.y = vp.m31 + vp.m01;
    f.planes[0].normal.z = vp.m32 + vp.m02;
    f.planes[0].distance = vp.m33 + vp.m03;

    // Right Plane (Column 3 - Row 0)
    f.planes[1].normal.x = vp.m30 - vp.m00;
    f.planes[1].normal.y = vp.m31 - vp.m01;
    f.planes[1].normal.z = vp.m32 - vp.m02;
    f.planes[1].distance = vp.m33 - vp.m03;

    // Bottom Plane (Row 3 + Row 1)
    f.planes[2].normal.x = vp.m30 + vp.m10;
    f.planes[2].normal.y = vp.m31 + vp.m11;
    f.planes[2].normal.z = vp.m32 + vp.m12;
    f.planes[2].distance = vp.m33 + vp.m13;

    // Top Plane (Row 3 - Row 1)
    f.planes[3].normal.x = vp.m30 - vp.m10;
    f.planes[3].normal.y = vp.m31 - vp.m11;
    f.planes[3].normal.z = vp.m32 - vp.m12;
    f.planes[3].distance = vp.m33 - vp.m13;

    // Near Plane (Row 3 + Row 2)
    f.planes[4].normal.x = vp.m30 + vp.m20;
    f.planes[4].normal.y = vp.m31 + vp.m21;
    f.planes[4].normal.z = vp.m32 + vp.m22;
    f.planes[4].distance = vp.m33 + vp.m23;

    // Far Plane (Row 3 - Row 2)
    f.planes[5].normal.x = vp.m30 - vp.m20;
    f.planes[5].normal.y = vp.m31 - vp.m21;
    f.planes[5].normal.z = vp.m32 - vp.m22;
    f.planes[5].distance = vp.m33 - vp.m23;

    // Normalize all planes
    for (int i = 0; i < 6; i++)
        NormalizePlane(&f.planes[i]);

    return f;
}





// Checks if a sphere is inside the frustum
bool Frustum_ContainsAABB(Frustum* frustum, AABB local_aabb, Matrix4 world_matrix)
{
    // Calculate local center and extents (half-sizes)
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

    // Transform the local center into a global center
    Vector3 center_world;
    center_world.x = world_matrix.m00 * center_local.x + world_matrix.m01 * center_local.y + world_matrix.m02 * center_local.z + world_matrix.m03;
    center_world.y = world_matrix.m10 * center_local.x + world_matrix.m11 * center_local.y + world_matrix.m12 * center_local.z + world_matrix.m13;
    center_world.z = world_matrix.m20 * center_local.x + world_matrix.m21 * center_local.y + world_matrix.m22 * center_local.z + world_matrix.m23;

    // Transform the local extents into global extents (Uses absolute rotation/scale values)
    Vector3 extents_world;
    extents_world.x = fabsf(world_matrix.m00) * extents_local.x + fabsf(world_matrix.m01) * extents_local.y + fabsf(world_matrix.m02) * extents_local.z;
    extents_world.y = fabsf(world_matrix.m10) * extents_local.x + fabsf(world_matrix.m11) * extents_local.y + fabsf(world_matrix.m12) * extents_local.z;
    extents_world.z = fabsf(world_matrix.m20) * extents_local.x + fabsf(world_matrix.m21) * extents_local.y + fabsf(world_matrix.m22) * extents_local.z;

    // Test the generated World AABB against the 6 planes
    for (int i = 0; i < 6; i++) 
    {
        FrustumPlane plane = frustum->planes[i];

        // Compute the "radius" of the AABB along this specific plane's normal
        float r = extents_world.x * fabsf(plane.normal.x) +
                  extents_world.y * fabsf(plane.normal.y) +
                  extents_world.z * fabsf(plane.normal.z);

        // Compute distance from the center of the AABB to the plane
        float d = plane.normal.x * center_world.x +
                  plane.normal.y * center_world.y +
                  plane.normal.z * center_world.z +
                  plane.distance;

        // If the distance is less than -r, the box is completely behind the plane
        if (d < -r)
            return false;
    }

    return true; // The box is visible
}










// Builds the eight world-space corners of a camera frustum slice.
void BuildFrustumSliceCorners(Vector3 cam_pos, Vector3 cam_fwd, Vector3 cam_right, Vector3 cam_up, float aspect, float tan_half, float split_near, float split_far, Vector3 corners[8])
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





// TODO - Deprecated function (Could be useful for something else)
// Builds a texel-snapped light-space matrix that fully contains the eight frustum corners of one cascade slice
// void ComputeCascadeLightMatrix(const Vector3 corners[8], Vector3 light_dir, Vector3 up, float light_distance, Matrix4* out_light_space, float* out_texel_world_size)
// {
//     Vector3 center = {0.0f, 0.0f, 0.0f};
//     for (int k = 0; k < 8; k++)
//     {
//         center.x += corners[k].x;
//         center.y += corners[k].y;
//         center.z += corners[k].z;
//     }
//     center.x *= 0.125f;
//     center.y *= 0.125f;
//     center.z *= 0.125f;

//     Vector3 light_pos = {
//         center.x - light_dir.x * light_distance,
//         center.y - light_dir.y * light_distance,
//         center.z - light_dir.z * light_distance
//     };
//     Matrix4 light_view = Matrix4LookAt(light_pos, center, up);

//     float min_x = FLT_MAX, max_x = -FLT_MAX;
//     float min_y = FLT_MAX, max_y = -FLT_MAX;

//     for (int k = 0; k < 8; k++)
//     {
//         Vector4 ls = Matrix4MultiplyVector4(light_view, (Vector4){corners[k].x, corners[k].y, corners[k].z, 1.0f});
//         if (ls.x < min_x) min_x = ls.x;
//         if (ls.x > max_x) max_x = ls.x;
//         if (ls.y < min_y) min_y = ls.y;
//         if (ls.y > max_y) max_y = ls.y;
//     }

//     // float texel_world_size = fmaxf(max_x - min_x, max_y - min_y) / (float)SHADOW_MAP_RESOLUTION;
//     RendererSettings cur_settings = Render_GetSettings(engine.renderer);
//     float cur_shadow_res = (float)(cur_settings.shadow_map_resolution > 0 ? cur_settings.shadow_map_resolution : SHADOW_MAP_RESOLUTION);
//     float texel_world_size = fmaxf(max_x - min_x, max_y - min_y) / cur_shadow_res;
//     if (texel_world_size <= 0.0f)
//         texel_world_size = 0.001f;

//     // Snap XY bounds outward to whole texels so shadows stay stable when the camera moves.
//     min_x = floorf(min_x / texel_world_size) * texel_world_size;
//     min_y = floorf(min_y / texel_world_size) * texel_world_size;
//     max_x = ceilf(max_x / texel_world_size) * texel_world_size;
//     max_y = ceilf(max_y / texel_world_size) * texel_world_size;

//     // Pull visible geometry away from the shadow-map UV edges (reduces PCF border leaks).
//     float edge_margin = texel_world_size * 4.0f;
//     min_x -= edge_margin;
//     min_y -= edge_margin;
//     max_x += edge_margin;
//     max_y += edge_margin;

//     Matrix4 light_proj = Matrix4Ortho(min_x, max_x, min_y, max_y, 0.1f, 2.0f * light_distance);
//     *out_light_space = Matrix4Multiply(light_proj, light_view);
//     *out_texel_world_size = texel_world_size;
// }




