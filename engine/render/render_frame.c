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