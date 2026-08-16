#include "scene.h"

#include "../core/input_core.h"
#include "../core/log_core.h"

#include <stdlib.h>
#include <string.h>



RetainedUIState g_ui_state = {0};



// Resets the UI context of a scene
void RetainedUI_Reset(Scene* scene)
{
    if (!scene)
        return;

    memset(scene->ui_canvases, 0, MAX_ENTITIES * sizeof(UICanvasComponent));
    memset(scene->ui_rect_transforms, 0, MAX_ENTITIES * sizeof(RectTransformComponent));
    memset(scene->ui_images, 0, MAX_ENTITIES * sizeof(UIImageComponent));
    memset(scene->ui_texts, 0, MAX_ENTITIES * sizeof(UITextComponent));
    memset(scene->ui_buttons, 0, MAX_ENTITIES * sizeof(UIButtonComponent));

    if (!g_ui_state.draw_list.vertices)
        OverlayDrawList_Init(&g_ui_state.draw_list);
    else
        OverlayDrawList_Reset(&g_ui_state.draw_list);
    
    g_ui_state.canvas_count = 0;
    g_ui_state.hovered_entity_id = ENTITY_NONE;
    g_ui_state.pressed_entity_id = ENTITY_NONE;
    g_ui_state.window_width = 0;
    g_ui_state.window_height = 0;
    g_ui_state.blocks_pointer = false;
    g_ui_state.layout_dirty = true;
}





// Shuts down the UI context in a scene
void RetainedUI_Shutdown(Scene* scene)
{
    OverlayDrawList_Free(&g_ui_state.draw_list);
    g_ui_state.canvas_count = 0;
}





// Consumes any mouse buttons events before updating the UI
void RetainedUI_PreUpdate(Scene* scene, uint32_t window_w, uint32_t window_h, float mouse_x, float mouse_y, bool mouse_captured)
{
    if (!scene)
        return;

    RetainedUI_UpdateLayout(scene, window_w, window_h);
    RetainedUI_ProcessPointer(scene, mouse_x, mouse_y, mouse_captured);

    if (g_ui_state.blocks_pointer)
        Input_ConsumeMouseButton(MOUSE_BUTTON_LEFT);
}





// Gathers all the UI Canvases in a scene
uint32_t RetainedUI_GatherCanvases(Scene* scene)
{
    if (!scene)
        return 0;

    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        const uint32_t required = COMPONENT_UI_CANVAS | COMPONENT_UI_RECT_TRANSFORM;
        if (!scene->is_active_in_hierarchy[i] || (scene->component_masks[i] & required) != required)
            continue;
        if (!scene->ui_canvases[i].is_active)
            continue;

        g_ui_state.canvas_entries[count].entity_id = i;
        g_ui_state.canvas_entries[count].sort_order = scene->ui_canvases[i].sort_order;
        count++;
    }

    g_ui_state.canvas_count = count;
    return count;
}















// Recursively marks all Rect Transforms in a scene as dirty
static void MarkRectDirtyRecursive(Scene* scene, uint32_t entity_id)
{
    if (!scene || entity_id == ENTITY_NONE || entity_id >= MAX_ENTITIES)
        return;

    if (scene->component_masks[entity_id] & COMPONENT_UI_RECT_TRANSFORM)
        scene->ui_rect_transforms[entity_id].is_dirty = true;

    uint32_t child = scene->transforms[entity_id].first_child_id;
    while (child != ENTITY_NONE)
    {
        MarkRectDirtyRecursive(scene, child);
        child = scene->transforms[child].next_sibling_id;
    }

    g_ui_state.layout_dirty = true;
}





// Marks the Rect Transform of an entity and its children as dirty
void RectTransform_MarkDirty(Entity entity)
{
    if (!Entity_IsValid(entity))
        return;

    MarkRectDirtyRecursive(entity.scene, entity.id);
}





// Sets the anchored position of an entities rect transform
void RectTransform_SetAnchoredPosition(Entity entity, Vector2 position)
{
    RectTransformComponent* rect = Entity_GetRectTransform(entity);
    if (!rect)
        return;

    rect->anchored_position = position;
    RectTransform_MarkDirty(entity);
}





// Sets the size delta of an entities rect transform
void RectTransform_SetSizeDelta(Entity entity, Vector2 size)
{
    RectTransformComponent* rect = Entity_GetRectTransform(entity);
    if (!rect)
        return;
    rect->size_delta = size;
    RectTransform_MarkDirty(entity);
}





// Sets the anchors of an entities rect transform
void RectTransform_SetAnchors(Entity entity, Vector2 min, Vector2 max)
{
    RectTransformComponent* rect = Entity_GetRectTransform(entity);
    if (!rect)
        return;
    rect->anchor_min = min;
    rect->anchor_max = max;
    RectTransform_MarkDirty(entity);
}





// Sets the pivot of an entities rect transform
void RectTransform_SetPivot(Entity entity, Vector2 pivot)
{
    RectTransformComponent* rect = Entity_GetRectTransform(entity);
    if (!rect)
        return;
    rect->pivot = pivot;
    RectTransform_MarkDirty(entity);
}















// Sets an entities UI Canvas as active
void UICanvas_SetActive(Entity entity, bool active)
{
    UICanvasComponent* canvas = Entity_GetUICanvas(entity);
    if (!canvas)
        return;

    canvas->is_active = active;
    g_ui_state.layout_dirty = true;
}





// Sets the scale mode of an entities UI Canvas
void UICanvas_SetScaleMode(Entity entity, UICanvasScaleMode mode)
{
    UICanvasComponent* canvas = Entity_GetUICanvas(entity);
    if (!canvas)
        return;
    canvas->scale_mode = mode;
    g_ui_state.layout_dirty = true;
}





// Sets the reference resolution of an entities UI Canvas
void UICanvas_SetReferenceResolution(Entity entity, Vector2 resolution)
{
    UICanvasComponent* canvas = Entity_GetUICanvas(entity);
    if (!canvas)
        return;
    canvas->reference_resolution = resolution;
    g_ui_state.layout_dirty = true;
}





// Sets whether a UI Canvas matches width or the height of the window
void UICanvas_SetMatchWidthOrHeight(Entity entity, float match)
{
    UICanvasComponent* canvas = Entity_GetUICanvas(entity);
    if (!canvas)
        return;
    if (match < 0.0f) match = 0.0f;
    if (match > 1.0f) match = 1.0f;

    canvas->match_width_or_height = match;
    g_ui_state.layout_dirty = true;
}















// Sets the text of a UI Text component
void UIText_SetText(Entity entity, const char* text)
{
    UITextComponent* ui_text = Entity_GetUIText(entity);
    if (!ui_text)
        return;

    ui_text->text[0] = '\0';
    if (text)
        strncpy(ui_text->text, text, 255);
    ui_text->text[255] = '\0';
}