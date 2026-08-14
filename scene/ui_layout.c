#include "ui_internal.h"





static float ComputeCanvasScale(const UICanvasComponent* canvas, uint32_t window_w, uint32_t window_h)
{
    if (canvas->scale_mode == UI_CANVAS_CONSTANT_PIXEL_SIZE)
        return 1.0f;

    float ref_w = canvas->reference_resolution.x > 0.0f ? canvas->reference_resolution.x : 1920.0f;
    float ref_h = canvas->reference_resolution.y > 0.0f ? canvas->reference_resolution.y : 1080.0f;
    float scale_w = (float)window_w / ref_w;
    float scale_h = (float)window_h / ref_h;
    float match = canvas->match_width_or_height;

    if (match < 0.0f) match = 0.0f;
    if (match > 1.0f) match = 1.0f;

    return scale_w * (1.0f - match) + scale_h * match;
}





// Lays out a child rect transform pased on the parent
static void LayoutRectChild(Scene* scene, uint32_t entity_id,
    const RectTransformComponent* parent_rect, float scale_factor, bool force)
{
    if (entity_id == ENTITY_NONE)
        return;

    RetainedUIContext* ui = scene->retained_ui;
    bool has_rect = (scene->component_masks[entity_id] & COMPONENT_UI_RECT_TRANSFORM) != 0;
    RectTransformComponent* rect = has_rect ? &ui->rect_transforms[entity_id] : NULL;
    bool needs_update = force || (rect && rect->is_dirty);

    if (rect && parent_rect && needs_update)
    {
        float anchor_x = parent_rect->screen_x + parent_rect->screen_width * rect->anchor_min.x;
        float anchor_y = parent_rect->screen_y + parent_rect->screen_height * rect->anchor_min.y;
        float anchor_w = parent_rect->screen_width * (rect->anchor_max.x - rect->anchor_min.x);
        float anchor_h = parent_rect->screen_height * (rect->anchor_max.y - rect->anchor_min.y);

        rect->screen_width = anchor_w + rect->size_delta.x * scale_factor;
        rect->screen_height = anchor_h + rect->size_delta.y * scale_factor;
        rect->screen_x = anchor_x + rect->anchored_position.x * scale_factor - rect->pivot.x * rect->screen_width;
        rect->screen_y = anchor_y + rect->anchored_position.y * scale_factor - rect->pivot.y * rect->screen_height;
        rect->is_dirty = false;
    }

    uint32_t child = scene->transforms[entity_id].first_child_id;
    while (child != ENTITY_NONE)
    {
        LayoutRectChild(scene, child, rect ? rect : parent_rect, scale_factor, needs_update);
        child = scene->transforms[child].next_sibling_id;
    }
}





// Updates the layout of the UI
void RetainedUI_UpdateLayoutInternal(Scene* scene, uint32_t window_w, uint32_t window_h)
{
    if (!scene || !scene->retained_ui || window_w == 0 || window_h == 0)
        return;

    RetainedUIContext* ui = scene->retained_ui;
    bool window_changed = ui->window_width != window_w || ui->window_height != window_h;
    if (!window_changed && !ui->layout_dirty)
        return;

    ui->window_width = window_w;
    ui->window_height = window_h;

    uint32_t canvas_count = RetainedUI_GatherCanvases(scene);
    for (uint32_t i = 0; i < canvas_count; i++)
    {
        uint32_t id = ui->canvas_entries[i].entity_id;
        UICanvasComponent* canvas = &ui->canvases[id];
        RectTransformComponent* rect = &ui->rect_transforms[id];

        canvas->scale_factor = ComputeCanvasScale(canvas, window_w, window_h);
        if (canvas->scale_factor <= 0.0f)
            canvas->scale_factor = 1.0f;

        rect->screen_x = 0.0f;
        rect->screen_y = 0.0f;
        rect->screen_width = (float)window_w;
        rect->screen_height = (float)window_h;
        rect->is_dirty = false;

        uint32_t child = scene->transforms[id].first_child_id;
        while (child != ENTITY_NONE)
        {
            LayoutRectChild(scene, child, rect, canvas->scale_factor, true);
            child = scene->transforms[child].next_sibling_id;
        }
    }

    ui->layout_dirty = false;
}