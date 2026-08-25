#ifndef SHADOW_CASCADES_H
#define SHADOW_CASCADES_H



#include "render.h"



// Primary camera used to fit directional shadow cascades
typedef struct ShadowCascadeCamera
{
    Vector3 position;
    Vector3 forward;
    Vector3 right;
    Vector3 up;
    float near_z;
    float far_z;
    float fov;
    float aspect;
} ShadowCascadeCamera;





// Backend-owned cascade matrices and split distances
typedef struct ShadowCascadeResult
{
    uint32_t cascade_count;
    Matrix4 light_space_matrices[MAX_SHADOW_CASCADES];
    float shadow_texel_world_sizes[MAX_SHADOW_CASCADES];
    float cascade_splits[MAX_SHADOW_CASCADES - 1];
    float cascade_blend_fraction;
} ShadowCascadeResult;





// Fits directional cascades from a primary camera and light. Does nothing if light is NULL.
void Render_ComputeDirectionalCascades(const DirectionalLightData* light, const ShadowCascadeCamera* camera, uint32_t shadow_map_resolution, ShadowCascadeResult* out);





#endif