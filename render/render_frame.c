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