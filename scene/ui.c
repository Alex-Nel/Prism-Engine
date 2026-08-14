#include "ui_internal.h"

#include "../core/input_core.h"
#include "../core/log_core.h"
#include "../render/render.h"

#include <stdlib.h>
#include <string.h>





// Frees the UI context
static void RetainedUI_FreeContext(RetainedUIContext* ui)
{
    if (!ui)
        return;

    OverlayDrawList_Free(&ui->draw_list);
    free(ui->canvas_entries);
    free(ui->canvases);
    free(ui->rect_transforms);
    free(ui->images);
    free(ui->texts);
    free(ui->buttons);
    free(ui);
}





// Initializes the UI context
RetainedUIContext* RetainedUI_EnsureContext(Scene* scene)
{
    if (!scene)
        return NULL;
    if (scene->retained_ui)
        return scene->retained_ui;

    RetainedUIContext* ui = (RetainedUIContext*)calloc(1, sizeof(RetainedUIContext));
    if (!ui)
        return NULL;

    ui->canvases = (UICanvasComponent*)calloc(MAX_ENTITIES, sizeof(UICanvasComponent));
    ui->rect_transforms = (RectTransformComponent*)calloc(MAX_ENTITIES, sizeof(RectTransformComponent));
    ui->images = (UIImageComponent*)calloc(MAX_ENTITIES, sizeof(UIImageComponent));
    ui->texts = (UITextComponent*)calloc(MAX_ENTITIES, sizeof(UITextComponent));
    ui->buttons = (UIButtonComponent*)calloc(MAX_ENTITIES, sizeof(UIButtonComponent));

    if (!ui->canvases || !ui->rect_transforms || !ui->images || !ui->texts || !ui->buttons)
    {
        Log_Error("ERROR: Failed to allocate retained UI scene storage");
        RetainedUI_FreeContext(ui);
        return NULL;
    }

    OverlayDrawList_Init(&ui->draw_list);
    ui->hovered_entity_id = ENTITY_NONE;
    ui->pressed_entity_id = ENTITY_NONE;
    ui->layout_dirty = true;
    scene->retained_ui = ui;

    return ui;
}





// Resets the UI context of a scene
void RetainedUI_Reset(Scene* scene)
{
    if (!scene || !scene->retained_ui)
        return;

    RetainedUIContext* ui = scene->retained_ui;
    memset(ui->canvases, 0, MAX_ENTITIES * sizeof(UICanvasComponent));
    memset(ui->rect_transforms, 0, MAX_ENTITIES * sizeof(RectTransformComponent));
    memset(ui->images, 0, MAX_ENTITIES * sizeof(UIImageComponent));
    memset(ui->texts, 0, MAX_ENTITIES * sizeof(UITextComponent));
    memset(ui->buttons, 0, MAX_ENTITIES * sizeof(UIButtonComponent));
    OverlayDrawList_Reset(&ui->draw_list);

    ui->canvas_count = 0;
    ui->hovered_entity_id = ENTITY_NONE;
    ui->pressed_entity_id = ENTITY_NONE;
    ui->window_width = 0;
    ui->window_height = 0;
    ui->blocks_pointer = false;
    ui->layout_dirty = true;
}





// Shuts down the UI context in a scene
void RetainedUI_Shutdown(Scene* scene)
{
    if (!scene)
        return;
    RetainedUI_FreeContext(scene->retained_ui);
    scene->retained_ui = NULL;
}





// Gathers all the UI Canvases in a scene
uint32_t RetainedUI_GatherCanvases(Scene* scene)
{
    if (!scene || !scene->retained_ui)
        return 0;

    RetainedUIContext* ui = scene->retained_ui;
    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        const uint32_t required = COMPONENT_UI_CANVAS | COMPONENT_UI_RECT_TRANSFORM;
        if (!scene->is_active_in_hierarchy[i] || (scene->component_masks[i] & required) != required)
            continue;
        if (!ui->canvases[i].is_active)
            continue;

        if (count == ui->canvas_capacity)
        {
            uint32_t new_capacity = ui->canvas_capacity ? ui->canvas_capacity * 2 : 8;
            UICanvasSortEntry* entries = (UICanvasSortEntry*)realloc(
                ui->canvas_entries, new_capacity * sizeof(UICanvasSortEntry));
            if (!entries)
                break;
            ui->canvas_entries = entries;
            ui->canvas_capacity = new_capacity;
        }

        ui->canvas_entries[count].entity_id = i;
        ui->canvas_entries[count].sort_order = ui->canvases[i].sort_order;
        count++;
    }

    ui->canvas_count = count;
    return count;
}





// Recursively marks all Rect Transforms in a scene as dirty
static void MarkRectDirtyRecursive(Scene* scene, uint32_t entity_id)
{
    if (!scene || !scene->retained_ui || entity_id == ENTITY_NONE || entity_id >= MAX_ENTITIES)
        return;

    RetainedUIContext* ui = scene->retained_ui;
    if (scene->component_masks[entity_id] & COMPONENT_UI_RECT_TRANSFORM)
        ui->rect_transforms[entity_id].is_dirty = true;

    uint32_t child = scene->transforms[entity_id].first_child_id;
    while (child != ENTITY_NONE)
    {
        MarkRectDirtyRecursive(scene, child);
        child = scene->transforms[child].next_sibling_id;
    }

    ui->layout_dirty = true;
}





// Marks the Rect Transform of an entity and its children as dirty
void RectTransform_MarkDirty(Entity entity)
{
    if (!Entity_IsValid(entity) || !entity.scene->retained_ui)
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






// Consumes any mouse buttons events before updating the UI
void RetainedUI_PreUpdate(Scene* scene, uint32_t window_w, uint32_t window_h, float mouse_x, float mouse_y, bool mouse_captured)
{
    if (!scene || !scene->retained_ui)
        return;

    RetainedUI_UpdateLayoutInternal(scene, window_w, window_h);
    RetainedUI_ProcessPointerInternal(scene, mouse_x, mouse_y, mouse_captured);

    if (scene->retained_ui->blocks_pointer)
        Input_ConsumeMouseButton(MOUSE_BUTTON_LEFT);
}





// Renders all the UI components in a scene
void RetainedUI_Render(Scene* scene, struct Renderer* renderer, uint32_t window_w, uint32_t window_h)
{
    if (!scene || !scene->retained_ui || !renderer)
        return;

    RetainedUI_UpdateLayoutInternal(scene, window_w, window_h);
    RetainedUI_BuildOverlayInternal(scene);
    Render_DrawOverlay(renderer, &scene->retained_ui->draw_list, window_w, window_h);
}





// Sets an entities UI Canvas as active
void UICanvas_SetActive(Entity entity, bool active)
{
    UICanvasComponent* canvas = Entity_GetUICanvas(entity);
    if (!canvas)
        return;

    canvas->is_active = active;
    entity.scene->retained_ui->layout_dirty = true;
}





// Sets the scale mode of an entities UI Canvas
void UICanvas_SetScaleMode(Entity entity, UICanvasScaleMode mode)
{
    UICanvasComponent* canvas = Entity_GetUICanvas(entity);
    if (!canvas)
        return;
    canvas->scale_mode = mode;
    entity.scene->retained_ui->layout_dirty = true;
}





// Sets the reference resolution of an entities UI Canvas
void UICanvas_SetReferenceResolution(Entity entity, Vector2 resolution)
{
    UICanvasComponent* canvas = Entity_GetUICanvas(entity);
    if (!canvas)
        return;
    canvas->reference_resolution = resolution;
    entity.scene->retained_ui->layout_dirty = true;
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
    entity.scene->retained_ui->layout_dirty = true;
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