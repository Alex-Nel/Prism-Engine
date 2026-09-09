#include "render_frame.h"
#include <string.h>





// Resets a render frame completely
void RenderFrame_Reset(RenderFrame* frame)
{
    if (!frame)
        return;

    frame->frame_id = 0;
    frame->width = 0;
    frame->height = 0;
    frame->dir_light_count = 0;
    frame->point_light_count = 0;
    frame->spot_light_count = 0;
    frame->reflection_probe_count = 0;
    frame->shadow_camera_pos = (Vector3){0};
    frame->camera_forward = (Vector3){0};
    frame->camera_right = (Vector3){0};
    frame->camera_up = (Vector3){0};
    frame->camera_near = 0.0f;
    frame->camera_far = 0.0f;
    frame->camera_fov = 0.0f;
    frame->camera_aspect = 0.0f;
    frame->enable_ssao = false;
    frame->global_ambient_color = (Color){0};
    frame->global_ambient_illumination = 0.0f;
    frame->gamma = 0.0f;
    frame->exposure = 0.0f;
    frame->env_map = (EnvironmentMapHandle){0};
    frame->has_probe_source_env_map = false;
    frame->probe_source_env_map = (EnvironmentMapHandle){0};
    frame->item_count = 0;
    frame->bone_slot_count = 0;
    frame->view_count = 0;
    OverlayDrawList_Reset(&frame->retained_ui);
    OverlayDrawList_Reset(&frame->immediate_ui);
}





// Fills a render frame with lighting information
void RenderFrame_FillLighting(const RenderFrame* frame, RenderLighting* out)
{
	if (!frame || !out)
        return;

    memset(out, 0, sizeof(RenderLighting));
    
    out->dir_lights = (DirectionalLightData*)frame->dir_lights;
    out->dir_light_count = frame->dir_light_count;
    
    out->point_lights = (PointLightData*)frame->point_lights;
    out->point_light_count = frame->point_light_count;
    
    out->spot_lights = (SpotLightData*)frame->spot_lights;
    out->spot_light_count = frame->spot_light_count;
    
    out->reflection_probes = frame->reflection_probes;
    out->reflection_probe_count = frame->reflection_probe_count;
    
    out->shadow_camera_pos = frame->shadow_camera_pos;
    out->camera_forward = frame->camera_forward;
    out->camera_right = frame->camera_right;
    out->camera_up = frame->camera_up;
    out->camera_near = frame->camera_near;
    out->camera_far = frame->camera_far;
    out->camera_fov = frame->camera_fov;
    out->camera_aspect = frame->camera_aspect;
    
    out->enable_ssao = frame->enable_ssao;
    out->global_ambient_color = frame->global_ambient_color;
    out->global_ambient_illumination = frame->global_ambient_illumination;
    out->gamma = frame->gamma;
    out->exposure = frame->exposure;
    out->env_map = frame->env_map;
    out->has_probe_source_env_map = frame->has_probe_source_env_map;
    out->probe_source_env_map = frame->probe_source_env_map;
}










// Initializes a render frame queue
bool RenderFrameQueue_Init(RenderFrameQueue* queue)
{
    if (!queue)
        return false;
    
    memset(queue, 0, sizeof(RenderFrameQueue));
    queue->mutex = Platform_CreateMutex();
    queue->frame_ready = Platform_CreateCondition();
    queue->slot_changed = Platform_CreateCondition();
    if (!queue->mutex || !queue->frame_ready || !queue->slot_changed)
    {
        RenderFrameQueue_Shutdown(queue);
        return false;
    }

    queue->slot_states[0] = RENDER_FRAME_SLOT_FREE;
    queue->slot_states[1] = RENDER_FRAME_SLOT_FREE;
    return true;
}





// Shuts down a render frame queue after its worker has joined
void RenderFrameQueue_Shutdown(RenderFrameQueue* queue)
{
    if (!queue)
        return;

    RenderFrameQueue_RequestStop(queue);
    OverlayDrawList_Free(&queue->buffers[0].retained_ui);
    OverlayDrawList_Free(&queue->buffers[0].immediate_ui);
    OverlayDrawList_Free(&queue->buffers[1].retained_ui);
    OverlayDrawList_Free(&queue->buffers[1].immediate_ui);

    if (queue->slot_changed)
        Platform_DestroyCondition(queue->slot_changed);
    if (queue->frame_ready)
        Platform_DestroyCondition(queue->frame_ready);
    
    if (queue->mutex)
        Platform_DestroyMutex(queue->mutex);

    queue->slot_changed = NULL;
    queue->frame_ready = NULL;
    queue->mutex = NULL;
}





static int RenderFrameQueue_FindState(const RenderFrameQueue* queue, RenderFrameSlotState state)
{
    for (uint32_t i = 0; i < 2; i++)
    {
        if (queue->slot_states[i] == state)
            return (int)i;
    }

    return -1;
}





static int RenderFrameQueue_FindNewestComplete(const RenderFrameQueue* queue)
{
    int newest = -1;
    for (uint32_t i = 0; i < 2; i++)
    {
        if (queue->slot_states[i] != RENDER_FRAME_SLOT_COMPLETE)
            continue;
        if (newest < 0 || queue->buffers[i].frame_id > queue->buffers[newest].frame_id)
            newest = (int)i;
    }
    return newest;
}





static int RenderFrameQueue_FindOldestComplete(const RenderFrameQueue* queue)
{
    int oldest = -1;
    for (uint32_t i = 0; i < 2; i++)
    {
        if (queue->slot_states[i] != RENDER_FRAME_SLOT_COMPLETE)
            continue;
        if (oldest < 0 || queue->results[i].frame_id < queue->results[oldest].frame_id)
            oldest = (int)i;
    }
    return oldest;
}





// Begins writing to a specific frame in a render frame queue
RenderFrame* RenderFrameQueue_BeginWrite(RenderFrameQueue* queue)
{
    if (!queue || !queue->mutex)
        return NULL;
    
    Platform_LockMutex(queue->mutex);
    int slot = RenderFrameQueue_FindState(queue, RENDER_FRAME_SLOT_FREE);
    
    while (slot < 0 && !queue->stopping)
    {
        Platform_WaitCondition(queue->slot_changed, queue->mutex);
        slot = RenderFrameQueue_FindState(queue, RENDER_FRAME_SLOT_FREE);
    }

    if (queue->stopping)
    {
        Platform_UnlockMutex(queue->mutex);
        return NULL;
    }
    
    queue->write_index = (uint32_t)slot;
    queue->slot_states[slot] = RENDER_FRAME_SLOT_WRITING;
    Platform_UnlockMutex(queue->mutex);
    
    return &queue->buffers[slot];
}





// Commits a write to a render queue frame
bool RenderFrameQueue_CommitWrite(RenderFrameQueue* queue, void* scene_identity)
{
    if (!queue || !queue->mutex)
        return false;
    
    Platform_LockMutex(queue->mutex);
    uint32_t slot = queue->write_index;
    if (queue->stopping || slot >= 2 || queue->slot_states[slot] != RENDER_FRAME_SLOT_WRITING)
    {
        Platform_UnlockMutex(queue->mutex);
        return false;
    }

    queue->scene_identities[slot] = scene_identity;
    queue->slot_states[slot] = RENDER_FRAME_SLOT_READY;
    Platform_SignalCondition(queue->frame_ready);
    Platform_UnlockMutex(queue->mutex);
    
    return true;
}





// Claims the oldest ready frame for the render thread
bool RenderFrameQueue_WaitRead(RenderFrameQueue* queue, uint32_t* slot_index, const RenderFrame** frame, bool* is_redraw, uint32_t* output_width, uint32_t* output_height)
{
    if (!queue || !slot_index || !frame || !is_redraw || !output_width || !output_height || !queue->mutex)
        return false;

    Platform_LockMutex(queue->mutex);
    int slot = RenderFrameQueue_FindState(queue, RENDER_FRAME_SLOT_READY);
    int redraw_slot = queue->redraw_requested ? RenderFrameQueue_FindNewestComplete(queue) : -1;
    
    while (slot < 0 && redraw_slot < 0 && !queue->stopping && !queue->wake_requested)
    {
        Platform_WaitCondition(queue->frame_ready, queue->mutex);
        slot = RenderFrameQueue_FindState(queue, RENDER_FRAME_SLOT_READY);
        redraw_slot = queue->redraw_requested ? RenderFrameQueue_FindNewestComplete(queue) : -1;
    }

    if (slot < 0 && redraw_slot >= 0)
    {
        slot = redraw_slot;
        queue->redraw_requested = false;
        *is_redraw = true;
        *output_width = queue->redraw_width;
        *output_height = queue->redraw_height;
    }
    else
    {
        *is_redraw = false;
    }
    
    if (slot < 0)
    {
        queue->wake_requested = false;
        Platform_UnlockMutex(queue->mutex);
        return false;
    }

    // When both slots are ready, preserve frame-id ordering.
    int other = slot == 0 ? 1 : 0;
    if (queue->slot_states[other] == RENDER_FRAME_SLOT_READY && queue->buffers[other].frame_id < queue->buffers[slot].frame_id)
        slot = other;

    queue->slot_states[slot] = RENDER_FRAME_SLOT_RENDERING;
    *slot_index = (uint32_t)slot;
    *frame = &queue->buffers[slot];
    
    if (!*is_redraw)
    {
        *output_width = queue->buffers[slot].width;
        *output_height = queue->buffers[slot].height;
    }
    
    Platform_UnlockMutex(queue->mutex);
    return true;
}





// Publishes the render result and leaves the frame immutable
void RenderFrameQueue_CompleteRead(RenderFrameQueue* queue, uint32_t slot_index, const RenderFrameResult* result)
{
    if (!queue || !result || slot_index >= 2 || !queue->mutex)
        return;

    Platform_LockMutex(queue->mutex);
    if (queue->slot_states[slot_index] == RENDER_FRAME_SLOT_RENDERING)
    {
        queue->results[slot_index] = *result;
        queue->results[slot_index].scene_identity = queue->scene_identities[slot_index];
        queue->slot_states[slot_index] = RENDER_FRAME_SLOT_COMPLETE;
        Platform_BroadcastCondition(queue->slot_changed);
        if (queue->redraw_requested)
            Platform_SignalCondition(queue->frame_ready);
    }
    
    Platform_UnlockMutex(queue->mutex);
}





// Claims a completed result for the main thread
bool RenderFrameQueue_AcquireCompleted(RenderFrameQueue* queue, bool wait, uint32_t* slot_index, const RenderFrameResult** result)
{
    if (!queue || !slot_index || !result || !queue->mutex)
        return false;

    Platform_LockMutex(queue->mutex);
    int slot = RenderFrameQueue_FindOldestComplete(queue);
    while (wait && slot < 0 && !queue->stopping)
    {
        Platform_WaitCondition(queue->slot_changed, queue->mutex);
        slot = RenderFrameQueue_FindOldestComplete(queue);
    }
    
    if (slot < 0)
    {
        Platform_UnlockMutex(queue->mutex);
        return false;
    }
    
    queue->slot_states[slot] = RENDER_FRAME_SLOT_APPLYING;
    *slot_index = (uint32_t)slot;
    *result = &queue->results[slot];
    
    Platform_UnlockMutex(queue->mutex);
    return true;
}





void RenderFrameQueue_ReleaseCompleted(RenderFrameQueue* queue, uint32_t slot_index)
{
    if (!queue || slot_index >= 2 || !queue->mutex)
        return;

    Platform_LockMutex(queue->mutex);
    if (queue->slot_states[slot_index] == RENDER_FRAME_SLOT_APPLYING)
    {
        queue->scene_identities[slot_index] = NULL;
        queue->slot_states[slot_index] = RENDER_FRAME_SLOT_FREE;
        Platform_BroadcastCondition(queue->slot_changed);
    }
    
    Platform_UnlockMutex(queue->mutex);
}





bool RenderFrameQueue_HasFreeSlot(RenderFrameQueue* queue)
{
    if (!queue || !queue->mutex)
        return false;

    Platform_LockMutex(queue->mutex);
    bool has_free_slot = RenderFrameQueue_FindState(queue, RENDER_FRAME_SLOT_FREE) >= 0;
    Platform_UnlockMutex(queue->mutex);
    
    return has_free_slot;
}





bool RenderFrameQueue_IsStopping(RenderFrameQueue* queue)
{
    if (!queue || !queue->mutex)
        return true;

    Platform_LockMutex(queue->mutex);
    bool stopping = queue->stopping;
    Platform_UnlockMutex(queue->mutex);
    
    return stopping;
}





void RenderFrameQueue_Wake(RenderFrameQueue* queue)
{
    if (!queue || !queue->mutex)
        return;

    Platform_LockMutex(queue->mutex);
    queue->wake_requested = true;
    Platform_SignalCondition(queue->frame_ready);
    Platform_UnlockMutex(queue->mutex);
}





void RenderFrameQueue_RequestRedraw(RenderFrameQueue* queue, uint32_t width, uint32_t height)
{
    if (!queue || !queue->mutex || width == 0 || height == 0)
        return;

    Platform_LockMutex(queue->mutex);
    queue->redraw_requested = true;
    queue->redraw_width = width;
    queue->redraw_height = height;
    Platform_SignalCondition(queue->frame_ready);
    Platform_UnlockMutex(queue->mutex);
}





void RenderFrameQueue_RequestStop(RenderFrameQueue* queue)
{
    if (!queue || !queue->mutex)
        return;
    Platform_LockMutex(queue->mutex);
    queue->stopping = true;
    queue->redraw_requested = false;
    Platform_BroadcastCondition(queue->frame_ready);
    Platform_BroadcastCondition(queue->slot_changed);
    Platform_UnlockMutex(queue->mutex);
}