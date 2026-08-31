#ifndef RENDER_FRAME_H
#define RENDER_FRAME_H



#include "render.h"
#include "../core/mesh_core.h"



#define RENDER_FRAME_MAX_VIEWS          8
#define RENDER_FRAME_MAX_ITEMS          32768
#define RENDER_FRAME_MAX_DIR_LIGHTS       4
#define RENDER_FRAME_MAX_POINT_LIGHTS   512
#define RENDER_FRAME_MAX_SPOT_LIGHTS    512
#define RENDER_FRAME_MAX_PROBES          16
#define RENDER_FRAME_MAX_SKINNED       1024



// Struct for a frame view
typedef struct RenderFrameView
{
    RenderView view;
    uint32_t item_start;
    uint32_t item_count;
} RenderFrameView;





// Self-contained snapshot built on the update side and consumed by the renderer.
typedef struct RenderFrame
{
    uint64_t frame_id;
    uint32_t width;
    uint32_t height;

    DirectionalLightData dir_lights[RENDER_FRAME_MAX_DIR_LIGHTS];
    uint32_t dir_light_count;
    
    PointLightData point_lights[RENDER_FRAME_MAX_POINT_LIGHTS];
    uint32_t point_light_count;
    
    SpotLightData spot_lights[RENDER_FRAME_MAX_SPOT_LIGHTS];
    uint32_t spot_light_count;
    
    ReflectionProbeData reflection_probes[RENDER_FRAME_MAX_PROBES];
    uint32_t reflection_probe_count;
    
    Vector3 shadow_camera_pos;
    Vector3 camera_forward;
    Vector3 camera_right;
    Vector3 camera_up;
    float camera_near;
    float camera_far;
    float camera_fov;
    float camera_aspect;
    
    bool enable_ssao;
    Color global_ambient_color;
    float global_ambient_illumination;
    float gamma;
    float exposure;
    EnvironmentMapHandle env_map;
    bool has_probe_source_env_map;
    EnvironmentMapHandle probe_source_env_map;
    
    RenderItem items[RENDER_FRAME_MAX_ITEMS];
    uint32_t item_count;
    
    Matrix4 bone_matrices[RENDER_FRAME_MAX_SKINNED][MAX_BONES];
    uint32_t bone_slot_count;
    
    RenderFrameView views[RENDER_FRAME_MAX_VIEWS];
    uint32_t view_count;
    
    RenderProbeResult probe_results[RENDER_FRAME_MAX_PROBES];
    uint32_t probe_result_count;
} RenderFrame;





// Resets a render frame completely
void RenderFrame_Reset(RenderFrame* frame);

// Fills a render frame with lighting information
void RenderFrame_FillLighting(const RenderFrame* frame, RenderLighting* out);





#endif // RENDER_FRAME_H