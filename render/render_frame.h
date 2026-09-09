#ifndef RENDER_FRAME_H
#define RENDER_FRAME_H



#include "render.h"
#include "../core/mesh_core.h"
#include "../platform/platform_core.h"



#define RENDER_FRAME_MAX_VIEWS            8
#define RENDER_FRAME_MAX_ITEMS_PER_VIEW   32768
#define RENDER_FRAME_MAX_ITEMS            (RENDER_FRAME_MAX_ITEMS_PER_VIEW * RENDER_FRAME_MAX_VIEWS)
#define RENDER_FRAME_MAX_DIR_LIGHTS       4
#define RENDER_FRAME_MAX_POINT_LIGHTS     512
#define RENDER_FRAME_MAX_SPOT_LIGHTS      512
#define RENDER_FRAME_MAX_PROBES           16
#define RENDER_FRAME_MAX_SKINNED          1024



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
    const Matrix4* bone_source_keys[RENDER_FRAME_MAX_SKINNED];
    uint32_t bone_slot_count;
    
    RenderFrameView views[RENDER_FRAME_MAX_VIEWS];
    uint32_t view_count;
    
    // Slot-owned UI snapshots. Their allocations are reused when the slot is reset.
    OverlayDrawList retained_ui;
    OverlayDrawList immediate_ui;
} RenderFrame;





// The render probe results from a render frame
typedef struct RenderFrameResult
{
    uint64_t frame_id;
    void* scene_identity;
    RenderProbeResult probe_results[RENDER_FRAME_MAX_PROBES];
    uint32_t probe_result_count;
} RenderFrameResult;





// Enum for the state of a render frame
typedef enum RenderFrameSlotState
{
    RENDER_FRAME_SLOT_FREE = 0,
    RENDER_FRAME_SLOT_WRITING,
    RENDER_FRAME_SLOT_READY,
    RENDER_FRAME_SLOT_RENDERING,
    RENDER_FRAME_SLOT_COMPLETE,
    RENDER_FRAME_SLOT_APPLYING
} RenderFrameSlotState;





// Struct for the render frame queue
typedef struct RenderFrameQueue
{
    RenderFrame buffers[2];
    RenderFrameResult results[2];
    RenderFrameSlotState slot_states[2];
    uint32_t write_index;
    void* scene_identities[2];
    bool stopping;
    bool wake_requested;
    bool redraw_requested;
    uint32_t redraw_width;
    uint32_t redraw_height;

    // Render-thread handoff variables
    PlatformMutex* mutex;
    PlatformCondition* frame_ready;
    PlatformCondition* slot_changed;
} RenderFrameQueue;





// Resets a render frame completely
void RenderFrame_Reset(RenderFrame* frame);

// Fills a render frame with lighting information
void RenderFrame_FillLighting(const RenderFrame* frame, RenderLighting* out);



// Initializes a render frame queue
bool RenderFrameQueue_Init(RenderFrameQueue* queue);

// Shuts down a render frame queue
void RenderFrameQueue_Shutdown(RenderFrameQueue* queue);

// Begins writing to a specific frame in a render frame queue
RenderFrame* RenderFrameQueue_BeginWrite(RenderFrameQueue* queue);

// Commits a write and wakes the render thread
bool RenderFrameQueue_CommitWrite(RenderFrameQueue* queue, void* scene_identity);

bool RenderFrameQueue_WaitRead(RenderFrameQueue* queue, uint32_t* slot_index, const RenderFrame** frame, bool* is_redraw, uint32_t* output_width, uint32_t* output_height);
void RenderFrameQueue_CompleteRead(RenderFrameQueue* queue, uint32_t slot_index, const RenderFrameResult* result);
bool RenderFrameQueue_AcquireCompleted(RenderFrameQueue* queue, bool wait, uint32_t* slot_index, const RenderFrameResult** result);
void RenderFrameQueue_ReleaseCompleted(RenderFrameQueue* queue, uint32_t slot_index);
bool RenderFrameQueue_HasFreeSlot(RenderFrameQueue* queue);
bool RenderFrameQueue_IsStopping(RenderFrameQueue* queue);
void RenderFrameQueue_Wake(RenderFrameQueue* queue);
void RenderFrameQueue_RequestRedraw(RenderFrameQueue* queue, uint32_t width, uint32_t height);
void RenderFrameQueue_RequestStop(RenderFrameQueue* queue);





#endif // RENDER_FRAME_H