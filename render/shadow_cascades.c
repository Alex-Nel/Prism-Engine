#include "shadow_cascades.h"
#include "../core/frustum_core.h"

#include <math.h>
#include <string.h>



static void FitOrthoLightMatrix(Vector3 center, Vector3 light_dir, Vector3 up, float shadow_box_size, float light_distance, float texel_world_size, Matrix4* out_light_space)
{
    Matrix4 light_basis = Matrix4LookAt((Vector3){0.0f, 0.0f, 0.0f}, light_dir, up);
    Vector4 center_ls = Matrix4MultiplyVector4(light_basis, (Vector4){center.x, center.y, center.z, 1.0f});
    center_ls.x = floorf(center_ls.x / texel_world_size) * texel_world_size;
    center_ls.y = floorf(center_ls.y / texel_world_size) * texel_world_size;

    Vector4 snapped = Matrix4MultiplyVector4(Matrix4Transpose(light_basis), center_ls);
    Vector3 center_snapped = { snapped.x, snapped.y, snapped.z };
    Vector3 light_pos = {
        center_snapped.x - light_dir.x * light_distance,
        center_snapped.y - light_dir.y * light_distance,
        center_snapped.z - light_dir.z * light_distance
    };
    
    Matrix4 light_view = Matrix4LookAt(light_pos, center_snapped, up);
    Matrix4 light_proj = Matrix4Ortho(-shadow_box_size, shadow_box_size,
                                      -shadow_box_size, shadow_box_size,
                                      0.1f, 2.0f * light_distance);
    
    *out_light_space = Matrix4Multiply(light_proj, light_view);
}










void Render_ComputeDirectionalCascades(const DirectionalLightData* light, const ShadowCascadeCamera* camera, uint32_t shadow_map_resolution, ShadowCascadeResult* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));
    out->cascade_count = 1;
    out->cascade_blend_fraction = 0.12f;
    
    if (!light || !camera)
        return;
    
    Vector3 light_dir = light->direction;
    Vector3 cam_pos = camera->position;
    Vector3 cam_fwd = camera->forward;
    Vector3 cam_right = camera->right;
    Vector3 cam_up = camera->up;
    float light_distance = 80.0f;
    Vector3 up;
    
    if (fabsf(light_dir.y) > 0.99f)
        up = (Vector3){0.0f, 0.0f, 1.0f};
    else
        up = (Vector3){0.0f, 1.0f, 0.0f};
    
    uint32_t cascade_count = light->shadow_cascade_count;
    if (cascade_count < 1)
        cascade_count = 1;
    if (cascade_count > MAX_SHADOW_CASCADES)
        cascade_count = MAX_SHADOW_CASCADES;
    
    out->cascade_count = cascade_count;
    float blend = light->cascade_blend_fraction;
    if (blend <= 0.0f)
        blend = 0.12f;
    if (blend > 0.5f)
        blend = 0.5f;
    
    out->cascade_blend_fraction = blend;
    
    float cur_shadow_res = (float)SHADOW_MAP_RESOLUTION;
    if (shadow_map_resolution > 0)
        cur_shadow_res = (float)shadow_map_resolution;
    
    if (cascade_count <= 1)
    {
        float shadow_box_size = light->shadow_box_size;
        if (shadow_box_size <= 0.0f)
            shadow_box_size = 20.0f;
        
        Vector3 center = {
            cam_pos.x + cam_fwd.x * shadow_box_size * 0.5f,
            cam_pos.y + cam_fwd.y * shadow_box_size * 0.5f,
            cam_pos.z + cam_fwd.z * shadow_box_size * 0.5f
        };
        
        float texel_world_size = (2.0f * shadow_box_size) / cur_shadow_res;
        FitOrthoLightMatrix(center, light_dir, up, shadow_box_size, light_distance, texel_world_size, &out->light_space_matrices[0]);
        out->shadow_texel_world_sizes[0] = texel_world_size;
        
        return;
    }
    
    float aspect = camera->aspect;
    if (aspect <= 0.0f)
        aspect = 1.0f;
    
    float cam_near = camera->near_z;
    float shadow_far = light->shadow_max_distance;
    
    if (shadow_far <= cam_near)
        shadow_far = cam_near + 1.0f;
    if (camera->far_z > 0.0f && shadow_far > camera->far_z)
        shadow_far = camera->far_z;
    
    float split_lambda = light->cascade_split_lambda;
    if (split_lambda < 0.0f)
        split_lambda = 0.0f;
    if (split_lambda > 1.0f)
        split_lambda = 1.0f;
    
    float splits[MAX_SHADOW_CASCADES - 1];
    for (uint32_t i = 1; i < cascade_count; i++)
    {
        float p = (float)i / (float)cascade_count;
        float log_split = cam_near * powf(shadow_far / cam_near, p);
        float uni_split = cam_near + (shadow_far - cam_near) * p;
        splits[i - 1] = uni_split * (1.0f - split_lambda) + log_split * split_lambda;
        out->cascade_splits[i - 1] = splits[i - 1];
    }
    
    float tan_half = tanf(camera->fov * 0.5f);
    for (uint32_t c = 0; c < cascade_count; c++)
    {
        float split_near;
        if (c == 0)
            split_near = cam_near;
        else
            split_near = splits[c - 1];
        
        float split_far;
        if (c == cascade_count - 1)
            split_far = shadow_far;
        else
            split_far = splits[c];
        
        if (c > 0)
        {
            float prev_near;
            if (c == 1)
                prev_near = cam_near;
            else
                prev_near = splits[c - 2];
            
            float prev_slice = splits[c - 1] - prev_near;
            split_near -= prev_slice * blend;
            if (split_near < cam_near)
                split_near = cam_near;
        }

        Vector3 corners[8];
        Frustum_BuildSliceCorners(cam_pos, cam_fwd, cam_right, cam_up, aspect, tan_half, split_near, split_far, corners);
        Vector3 center = {0, 0, 0};
        for (int k = 0; k < 8; k++)
        {
            center.x += corners[k].x;
            center.y += corners[k].y;
            center.z += corners[k].z;
        }
        center.x /= 8.0f;
        center.y /= 8.0f;
        center.z /= 8.0f;
        
        float radius = 0.0f;
        for (int k = 0; k < 8; k++)
        {
            float dx = corners[k].x - center.x;
            float dy = corners[k].y - center.y;
            float dz = corners[k].z - center.z;
            float dist = sqrtf(dx * dx + dy * dy + dz * dz);
            if (dist > radius)
                radius = dist;
        }

        radius = ceilf(radius / 16.0f) * 16.0f;
        float shadow_box_size = radius;
        float texel_world_size = (2.0f * shadow_box_size) / cur_shadow_res;
        FitOrthoLightMatrix(center, light_dir, up, shadow_box_size, light_distance, texel_world_size, &out->light_space_matrices[c]);
        
        out->shadow_texel_world_sizes[c] = texel_world_size;
    }
}