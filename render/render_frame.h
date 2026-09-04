#ifndef RENDER_FRAME_H
#define RENDER_FRAME_H



#include "render.h"
#include "../core/mesh_core.h"
#include "../platform/platform_core.h"



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





// Struct for the render frame queue
typedef struct RenderFrameQueue
{
    RenderFrame buffers[2];
    uint32_t write_index;
    uint32_t read_index;

    // Render-thread handoff variables
    PlatformMutex* mutex;
    PlatformCondition* frame_ready;
} RenderFrameQueue;





// Resets a render frame completely
void RenderFrame_Reset(RenderFrame* frame);

// Fills a render frame with lighting information
void RenderFrame_FillLighting(const RenderFrame* frame, RenderLighting* out);



// Initializes a render frame queue
void RenderFrameQueue_Init(RenderFrameQueue* queue);

// Shuts down a render frame queue
void RenderFrameQueue_Shutdown(RenderFrameQueue* queue);

// Begins writing to a specific frame in a render frame queue
RenderFrame* RenderFrameQueue_BeginWrite(RenderFrameQueue* queue);

// Commits a write to a reneder queue
RenderFrame* RenderFrameQueue_CommitWrite(RenderFrameQueue* queue);

// Returns the read information from a frame in the render frame queue
const RenderFrame* RenderFrameQueue_GetReadFrame(const RenderFrameQueue* queue);





#endif // RENDER_FRAME_H