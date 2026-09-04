#include "render_frame.h"
#include <string.h>





// Resets a render frame completely
void RenderFrame_Reset(RenderFrame* frame)
{
    if (!frame)
        return;
    memset(frame, 0, sizeof(RenderFrame));
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
void RenderFrameQueue_Init(RenderFrameQueue* queue)
{
    if (!queue)
        return;
    
    memset(queue, 0, sizeof(RenderFrameQueue));
    queue->write_index = 0;
    queue->read_index = 1;

    queue->mutex = Platform_CreateMutex();
    queue->frame_ready = Platform_CreateCondition();
}





// Shuts down a render frame queue
void RenderFrameQueue_Shutdown(RenderFrameQueue* queue)
{
    if (!queue)
        return;

    if (queue->frame_ready)
    {
        Platform_DestroyCondition(queue->frame_ready);
        queue->frame_ready = NULL;
    }
    
    if (queue->mutex)
    {
        Platform_DestroyMutex(queue->mutex);
        queue->mutex = NULL;
    }
}





// Begins writing to a specific frame in a render frame queue
RenderFrame* RenderFrameQueue_BeginWrite(RenderFrameQueue* queue)
{
    if (!queue)
        return NULL;
    
    return &queue->buffers[queue->write_index];
}





// Commits a write to a render queue frame
RenderFrame* RenderFrameQueue_CommitWrite(RenderFrameQueue* queue)
{
    if (!queue)
        return NULL;
    
    uint32_t next_write = queue->read_index;
    queue->read_index = queue->write_index;
    queue->write_index = next_write;
    return &queue->buffers[queue->read_index];
}





// Returns the read information from a frame in the render frame queue
const RenderFrame* RenderFrameQueue_GetReadFrame(const RenderFrameQueue* queue)
{
    if (!queue)
        return NULL;
    
    return &queue->buffers[queue->read_index];
}