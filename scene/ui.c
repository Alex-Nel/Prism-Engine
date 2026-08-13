#include "scene.h"
#include "../core/input_core.h"
#include <math.h>



#define MAX_UI_CANVASES 64



typedef struct UICanvasSortEntry
{
    uint32_t entity_id;
    int sort_order;
} UICanvasSortEntry;





// Converts a color channel to a byte
static uint8_t ColorChannelToByte(float c)
{
    if (c < 0.0f) c = 0.0f;
    if (c > 1.0f) c = 1.0f;

    return (uint8_t)(c * 255.0f + 0.5f);
}





// Multiplies two colors together
static Color ColorMultiply(Color a, Color b)
{
    return (Color){ a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a };
}





// Function to compare two canvases based on draw order
static int CompareCanvasDrawOrder(const void* a, const void* b)
{
    return ((const UICanvasSortEntry*)a)->sort_order - ((const UICanvasSortEntry*)b)->sort_order;
}





// Function to compare two canvases based on hit order
static int CompareCanvasHitOrder(const void* a, const void* b)
{
    return ((const UICanvasSortEntry*)b)->sort_order - ((const UICanvasSortEntry*)a)->sort_order;
}





// Gathers all the UI Canvases in the scene
static uint32_t GatherCanvases(Scene* scene, UICanvasSortEntry* out_entries, uint32_t max_count)
{
    uint32_t count = 0;
    const uint32_t required = COMPONENT_UI_CANVAS | COMPONENT_UI_RECT_TRANSFORM;

    for (uint32_t i = 0; i < MAX_ENTITIES && count < max_count; i++)
    {
        if (!scene->is_active_in_hierarchy[i])
            continue;
        if ((scene->component_masks[i] & required) != required)
            continue;
        if (!scene->ui_canvases[i].is_active)
            continue;
        if (scene->ui_canvases[i].render_mode != UI_CANVAS_OVERLAY)
            continue;

        out_entries[count].entity_id = i;
        out_entries[count].sort_order = scene->ui_canvases[i].sort_order;
        count++;
    }

    return count;
}





// Recursively marks rect transforms as dirty
static void MarkRectDirtyRecursive(Scene* scene, uint32_t entity_id)
{
    if (entity_id == ENTITY_NONE)
        return;

    if (scene->component_masks[entity_id] & COMPONENT_UI_RECT_TRANSFORM)
        scene->rect_transforms[entity_id].is_dirty = true;

    uint32_t child = scene->transforms[entity_id].first_child_id;
    while (child != ENTITY_NONE)
    {
        MarkRectDirtyRecursive(scene, child);
        child = scene->transforms[child].next_sibling_id;
    }
}





// Marks a rect transform as dirty
void RectTransform_MarkDirty(Entity entity)
{
    if (!Entity_IsValid(entity))
        return;

    MarkRectDirtyRecursive(entity.scene, entity.id);
}





// If a rect transform is present, it gets marked as dirty down the tree
static void RectTransform_DirtyIfPresent(Entity entity)
{
    if (!Entity_IsValid(entity))
        return;
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_UI_RECT_TRANSFORM))
        return;
    MarkRectDirtyRecursive(entity.scene, entity.id);
}





// Sets the anchored position of a rect transform
void RectTransform_SetAnchoredPosition(Entity entity, Vector2 position)
{
    RectTransformComponent* rect = Entity_GetRectTransform(entity);
    if (!rect)
        return;

    rect->anchored_position = position;
    RectTransform_MarkDirty(entity);
}





// Sets the size delta of a rect transform
void RectTransform_SetSizeDelta(Entity entity, Vector2 size)
{
    RectTransformComponent* rect = Entity_GetRectTransform(entity);
    if (!rect)
        return;

    rect->size_delta = size;
    RectTransform_MarkDirty(entity);
}





// Sets the anchors of a rect transform
void RectTransform_SetAnchors(Entity entity, Vector2 min, Vector2 max)
{
    RectTransformComponent* rect = Entity_GetRectTransform(entity);
    if (!rect)
        return;

    rect->anchor_min = min;
    rect->anchor_max = max;
    RectTransform_MarkDirty(entity);
}





// Sets the pivot of a rect transform
void RectTransform_SetPivot(Entity entity, Vector2 pivot)
{
    RectTransformComponent* rect = Entity_GetRectTransform(entity);
    if (!rect)
        return;

    rect->pivot = pivot;
    RectTransform_MarkDirty(entity);
}





// Sets the local scale of a rect transform
void RectTransform_SetLocalScale(Entity entity, Vector2 scale)
{
    RectTransformComponent* rect = Entity_GetRectTransform(entity);
    if (!rect)
        return;

    rect->local_scale = scale;
    RectTransform_MarkDirty(entity);
}





// Sets the local Z rotation of a rect transform
void RectTransform_SetLocalRotationZ(Entity entity, float degrees)
{
    RectTransformComponent* rect = Entity_GetRectTransform(entity);
    if (!rect)
        return;

    rect->local_rotation_z = degrees;
    RectTransform_MarkDirty(entity);
}





// Computes the scale of a UI canvas
static float ComputeCanvasScale(const UICanvasComponent* canvas, uint32_t window_w, uint32_t window_h)
{
    if (canvas->scale_mode == UI_CANVAS_CONSTANT_PIXEL_SIZE)
        return 1.0f;

    float ref_w = canvas->reference_resolution.x > 0.0f ? canvas->reference_resolution.x : 1920.0f;
    float ref_h = canvas->reference_resolution.y > 0.0f ? canvas->reference_resolution.y : 1080.0f;
    float scale_w = (float)window_w / ref_w;
    float scale_h = (float)window_h / ref_h;
    float t = canvas->match_width_or_height;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    return scale_w * (1.0f - t) + scale_h * t;
}





// Lays out the rect transform of a child
static void LayoutRectChild(Scene* scene, uint32_t entity_id, const RectTransformComponent* parent_rect, float scale_factor, bool force)
{
    if (entity_id == ENTITY_NONE)
        return;

    bool has_rect = (scene->component_masks[entity_id] & COMPONENT_UI_RECT_TRANSFORM) != 0;
    RectTransformComponent* rect = has_rect ? &scene->rect_transforms[entity_id] : NULL;
    bool needs_update = force || (rect && rect->is_dirty);

    if (rect && needs_update && parent_rect)
    {
        float anchor_x = parent_rect->screen_x + parent_rect->screen_width * rect->anchor_min.x;
        float anchor_y = parent_rect->screen_y + parent_rect->screen_height * rect->anchor_min.y;
        float anchor_w = parent_rect->screen_width * (rect->anchor_max.x - rect->anchor_min.x);
        float anchor_h = parent_rect->screen_height * (rect->anchor_max.y - rect->anchor_min.y);

        float width = (anchor_w + rect->size_delta.x * scale_factor) * rect->local_scale.x;
        float height = (anchor_h + rect->size_delta.y * scale_factor) * rect->local_scale.y;

        rect->screen_width = width;
        rect->screen_height = height;
        rect->screen_x = anchor_x + rect->anchored_position.x * scale_factor - rect->pivot.x * width;
        rect->screen_y = anchor_y + rect->anchored_position.y * scale_factor - rect->pivot.y * height;
        rect->is_dirty = false;
    }
    else if (rect)
    {
        rect->is_dirty = false;
    }

    uint32_t child = scene->transforms[entity_id].first_child_id;
    while (child != ENTITY_NONE)
    {
        LayoutRectChild(scene, child, rect ? rect : parent_rect, scale_factor, needs_update);
        child = scene->transforms[child].next_sibling_id;
    }
}





// Updates the UI layout in the scene
void Scene_UpdateUILayout(Scene* scene, uint32_t window_w, uint32_t window_h)
{
    if (!scene || window_w == 0 || window_h == 0)
        return;

    bool window_changed = (scene->ui_window_width != window_w || scene->ui_window_height != window_h);
    scene->ui_window_width = window_w;
    scene->ui_window_height = window_h;

    UICanvasSortEntry canvases[MAX_UI_CANVASES];
    uint32_t canvas_count = GatherCanvases(scene, canvases, MAX_UI_CANVASES);

    for (uint32_t i = 0; i < canvas_count; i++)
    {
        uint32_t id = canvases[i].entity_id;
        UICanvasComponent* canvas = &scene->ui_canvases[id];
        RectTransformComponent* rect = &scene->rect_transforms[id];

        canvas->scale_factor = ComputeCanvasScale(canvas, window_w, window_h);
        if (canvas->scale_factor <= 0.0f)
            canvas->scale_factor = 1.0f;

        bool force = window_changed || rect->is_dirty;
        if (canvas->scale_mode == UI_CANVAS_SCALE_WITH_SCREEN_SIZE)
        {
            float scaled_w = canvas->reference_resolution.x * canvas->scale_factor;
            float scaled_h = canvas->reference_resolution.y * canvas->scale_factor;
            if (scaled_w <= 0.0f) scaled_w = (float)window_w;
            if (scaled_h <= 0.0f) scaled_h = (float)window_h;
            rect->screen_width = scaled_w;
            rect->screen_height = scaled_h;
            rect->screen_x = ((float)window_w - scaled_w) * 0.5f;
            rect->screen_y = ((float)window_h - scaled_h) * 0.5f;
        }
        else
        {
            rect->screen_x = 0.0f;
            rect->screen_y = 0.0f;
            rect->screen_width = (float)window_w;
            rect->screen_height = (float)window_h;
        }
        rect->is_dirty = false;

        uint32_t child = scene->transforms[id].first_child_id;
        while (child != ENTITY_NONE)
        {
            LayoutRectChild(scene, child, rect, canvas->scale_factor, force);
            child = scene->transforms[child].next_sibling_id;
        }
    }
}





// Returns whether a specific point is on a rect transform
static bool PointInRect(const RectTransformComponent* rect, float x, float y)
{
    return x >= rect->screen_x && y >= rect->screen_y &&
           x <= rect->screen_x + rect->screen_width &&
           y <= rect->screen_y + rect->screen_height;
}





// Returns whether an entity blocks a raycast
static bool EntityBlocksRaycast(Scene* scene, uint32_t id)
{
    if (!scene->is_active_in_hierarchy[id])
        return false;
    if (!(scene->component_masks[id] & COMPONENT_UI_RECT_TRANSFORM))
        return false;

    bool has_button = (scene->component_masks[id] & COMPONENT_UI_BUTTON) != 0;
    if (has_button)
    {
        UIButtonComponent* button = &scene->ui_buttons[id];
        return button->is_active && button->interactable;
    }

    if ((scene->component_masks[id] & COMPONENT_UI_IMAGE) && scene->ui_images[id].is_active && scene->ui_images[id].raycast_target)
        return true;
    if ((scene->component_masks[id] & COMPONENT_UI_TEXT) && scene->ui_texts[id].is_active && scene->ui_texts[id].raycast_target)
        return true;

    return false;
}





// Tests whether a transform tree is hit
static uint32_t HitTestTree(Scene* scene, uint32_t entity_id, float mouse_x, float mouse_y)
{
    if (entity_id == ENTITY_NONE || !scene->is_active_in_hierarchy[entity_id])
        return ENTITY_NONE;

    uint32_t child = scene->transforms[entity_id].first_child_id;
    uint32_t last_child = ENTITY_NONE;
    while (child != ENTITY_NONE)
    {
        last_child = child;
        child = scene->transforms[child].next_sibling_id;
    }

    child = last_child;
    while (child != ENTITY_NONE)
    {
        uint32_t hit = HitTestTree(scene, child, mouse_x, mouse_y);
        if (hit != ENTITY_NONE)
            return hit;
        child = scene->transforms[child].prev_sibling_id;
    }

    if ((scene->component_masks[entity_id] & COMPONENT_UI_RECT_TRANSFORM) &&
        PointInRect(&scene->rect_transforms[entity_id], mouse_x, mouse_y) &&
        EntityBlocksRaycast(scene, entity_id))
    {
        return entity_id;
    }

    return ENTITY_NONE;
}





// Fires the pointer callback for any scripts attached
static void FirePointer(Scene* scene, uint32_t entity_id, int kind)
{
    if (entity_id == ENTITY_NONE)
        return;
    if (!(scene->component_masks[entity_id] & COMPONENT_SCRIPT))
        return;

    ScriptComponent* scripts = &scene->scripts[entity_id];
    Entity self = { entity_id, scene };

    for (uint32_t s = 0; s < scripts->count; s++)
    {
        ScriptInstance* script = &scripts->instances[s];
        if (!script->is_active)
            continue;

        PointerCallback cb = NULL;
        if (kind == 0) cb = script->OnPointerEnter;
        else if (kind == 1) cb = script->OnPointerExit;
        else if (kind == 2) cb = script->OnPointerDown;
        else if (kind == 3) cb = script->OnPointerUp;
        else if (kind == 4) cb = script->OnPointerClick;

        if (cb)
            cb(self, script->instance_data);
    }
}





// Updates the button state of a button
static void UpdateButtonState(Scene* scene, uint32_t id, bool hovered, bool pressed)
{
    if (!(scene->component_masks[id] & COMPONENT_UI_BUTTON))
        return;

    UIButtonComponent* button = &scene->ui_buttons[id];
    if (!button->is_active || !button->interactable)
    {
        button->current_state = UI_BUTTON_STATE_DISABLED;
        return;
    }

    if (pressed)
        button->current_state = UI_BUTTON_STATE_PRESSED;
    else if (hovered)
        button->current_state = UI_BUTTON_STATE_HOVERED;
    else
        button->current_state = UI_BUTTON_STATE_NORMAL;
}





// Process a UI pointer event on every UI component
void Scene_ProcessUIPointer(Scene* scene, float mouse_x, float mouse_y, bool mouse_captured)
{
    if (!scene)
        return;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (scene->component_masks[i] & COMPONENT_UI_BUTTON)
            scene->ui_buttons[i].clicked_this_frame = false;
    }

    if (mouse_captured)
    {
        if (scene->ui_hovered_entity_id != ENTITY_NONE)
            FirePointer(scene, scene->ui_hovered_entity_id, 1);
        scene->ui_hovered_entity_id = ENTITY_NONE;
        scene->ui_pressed_entity_id = ENTITY_NONE;
        scene->ui_blocks_pointer = false;
        return;
    }

    UICanvasSortEntry canvases[MAX_UI_CANVASES];
    uint32_t canvas_count = GatherCanvases(scene, canvases, MAX_UI_CANVASES);
    qsort(canvases, canvas_count, sizeof(UICanvasSortEntry), CompareCanvasHitOrder);

    uint32_t hit = ENTITY_NONE;
    for (uint32_t i = 0; i < canvas_count; i++)
    {
        uint32_t canvas_id = canvases[i].entity_id;
        if (!scene->ui_canvases[canvas_id].blocks_raycasts)
            continue;

        hit = HitTestTree(scene, canvas_id, mouse_x, mouse_y);
        if (hit != ENTITY_NONE)
            break;
    }

    scene->ui_blocks_pointer = (hit != ENTITY_NONE);

    if (hit != scene->ui_hovered_entity_id)
    {
        FirePointer(scene, scene->ui_hovered_entity_id, 1);
        FirePointer(scene, hit, 0);
        scene->ui_hovered_entity_id = hit;
    }

    bool pressed = Input_IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool down = Input_IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool released = Input_IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    if (pressed && hit != ENTITY_NONE)
    {
        scene->ui_pressed_entity_id = hit;
        FirePointer(scene, hit, 2);
    }

    if (released)
    {
        if (scene->ui_pressed_entity_id != ENTITY_NONE)
        {
            FirePointer(scene, scene->ui_pressed_entity_id, 3);
            if (scene->ui_pressed_entity_id == hit && (scene->component_masks[hit] & COMPONENT_UI_BUTTON))
            {
                UIButtonComponent* button = &scene->ui_buttons[hit];
                if (button->is_active && button->interactable)
                {
                    button->clicked_this_frame = true;
                    FirePointer(scene, hit, 4);
                }
            }
        }
        scene->ui_pressed_entity_id = ENTITY_NONE;
    }

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!(scene->component_masks[i] & COMPONENT_UI_BUTTON))
            continue;
        bool is_hover = (i == hit);
        bool is_press = (i == scene->ui_pressed_entity_id) && down;
        UpdateButtonState(scene, i, is_hover, is_press);
    }
}





// Returns whether the UI blocks the pointer
bool Scene_UIBlocksPointer(Scene* scene)
{
    return scene && scene->ui_blocks_pointer;
}





// Rotates a point by a certain amount of radians
static void RotatePoint(float cx, float cy, float angle_rad, float* x, float* y)
{
    float dx = *x - cx;
    float dy = *y - cy;
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    *x = cx + dx * c - dy * s;
    *y = cy + dx * s + dy * c;
}





// Puts an image into the overlay draw list
static void EmitImage(Scene* scene, uint32_t id, OverlayDrawList* list, float clip_x, float clip_y, float clip_w, float clip_h)
{
    if (!(scene->component_masks[id] & COMPONENT_UI_IMAGE))
        return;

    UIImageComponent* image = &scene->ui_images[id];
    if (!image->is_active)
        return;

    RectTransformComponent* rect = &scene->rect_transforms[id];
    Color color = image->color;
    if (scene->component_masks[id] & COMPONENT_UI_BUTTON)
    {
        UIButtonComponent* button = &scene->ui_buttons[id];
        Color tint = button->color_normal;
        if (!button->interactable || !button->is_active)
            tint = button->color_disabled;
        else if (button->current_state == UI_BUTTON_STATE_PRESSED)
            tint = button->color_pressed;
        else if (button->current_state == UI_BUTTON_STATE_HOVERED)
            tint = button->color_hovered;
        color = ColorMultiply(color, tint);
    }

    TextureHandle texture = image->texture ? image->texture->gpu_handle : (TextureHandle){0};
    uint8_t r = ColorChannelToByte(color.r);
    uint8_t g = ColorChannelToByte(color.g);
    uint8_t b = ColorChannelToByte(color.b);
    uint8_t a = ColorChannelToByte(color.a);

    float x0 = rect->screen_x;
    float y0 = rect->screen_y;
    float x1 = rect->screen_x + rect->screen_width;
    float y1 = rect->screen_y;
    float x2 = rect->screen_x + rect->screen_width;
    float y2 = rect->screen_y + rect->screen_height;
    float x3 = rect->screen_x;
    float y3 = rect->screen_y + rect->screen_height;

    if (rect->local_rotation_z != 0.0f)
    {
        float cx = rect->screen_x + rect->pivot.x * rect->screen_width;
        float cy = rect->screen_y + rect->pivot.y * rect->screen_height;
        float angle = rect->local_rotation_z * (3.14159265f / 180.0f);
        RotatePoint(cx, cy, angle, &x0, &y0);
        RotatePoint(cx, cy, angle, &x1, &y1);
        RotatePoint(cx, cy, angle, &x2, &y2);
        RotatePoint(cx, cy, angle, &x3, &y3);
    }

    OverlayDrawList_AddQuad(list, x0, y0, x1, y1, x2, y2, x3, y3,
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
        r, g, b, a, texture, clip_x, clip_y, clip_w, clip_h);
}





// Puts text into the overlay draw list
static void EmitText(Scene* scene, uint32_t id, OverlayDrawList* list, float clip_x, float clip_y, float clip_w, float clip_h)
{
    if (!(scene->component_masks[id] & COMPONENT_UI_TEXT))
        return;

    UITextComponent* text = &scene->ui_texts[id];
    if (!text->is_active || !text->font || !text->font->texture_atlas || text->text[0] == '\0')
        return;

    RectTransformComponent* rect = &scene->rect_transforms[id];
    Font* font = text->font;
    float scale = (font->size > 0.0f) ? (text->font_size / font->size) : 1.0f;
    float wrap_width = text->wrap ? rect->screen_width : 0.0f;
    float line_height = font->line_height * scale;
    if (line_height <= 0.0f)
        line_height = text->font_size;

    uint8_t r = ColorChannelToByte(text->color.r);
    uint8_t g = ColorChannelToByte(text->color.g);
    uint8_t b = ColorChannelToByte(text->color.b);
    uint8_t a = ColorChannelToByte(text->color.a);
    TextureHandle texture = font->texture_atlas->gpu_handle;

    float text_clip_x = rect->screen_x;
    float text_clip_y = rect->screen_y;
    float text_clip_w = rect->screen_width;
    float text_clip_h = rect->screen_height;

    float intersect_x = text_clip_x > clip_x ? text_clip_x : clip_x;
    float intersect_y = text_clip_y > clip_y ? text_clip_y : clip_y;
    float intersect_r = (text_clip_x + text_clip_w) < (clip_x + clip_w) ? (text_clip_x + text_clip_w) : (clip_x + clip_w);
    float intersect_b = (text_clip_y + text_clip_h) < (clip_y + clip_h) ? (text_clip_y + text_clip_h) : (clip_y + clip_h);
    float intersect_w = intersect_r - intersect_x;
    float intersect_h = intersect_b - intersect_y;
    if (intersect_w <= 0.0f || intersect_h <= 0.0f)
        return;

    const char* str = text->text;
    float cursor_y = rect->screen_y + font->ascent * scale;

    while (*str)
    {
        const char* line_start = str;
        float line_width = 0.0f;
        const char* line_end = str;

        while (*line_end && *line_end != '\n')
        {
            const char* word_end = line_end;
            float word_width = 0.0f;
            if (wrap_width > 0.0f && *word_end == ' ')
            {
                float adv = 0.0f;
                float x = 0.0f, y = 0.0f, x0, y0, x1, y1, u0, v0, u1, v1;
                Font_GetGlyphQuad(font, ' ', &x, &y, scale, &x0, &y0, &x1, &y1, &u0, &v0, &u1, &v1);
                adv = x;
                if (line_width + adv > wrap_width && line_width > 0.0f)
                    break;
                line_width += adv;
                line_end++;
                continue;
            }

            while (*word_end && *word_end != '\n' && *word_end != ' ')
            {
                float x = 0.0f, y = 0.0f, x0, y0, x1, y1, u0, v0, u1, v1;
                if (Font_GetGlyphQuad(font, (unsigned char)*word_end, &x, &y, scale, &x0, &y0, &x1, &y1, &u0, &v0, &u1, &v1))
                    word_width += x;
                word_end++;
            }

            if (wrap_width > 0.0f && line_width + word_width > wrap_width && line_width > 0.0f)
                break;

            line_width += word_width;
            line_end = word_end;
        }

        float cursor_x = rect->screen_x;
        if (text->alignment == UI_TEXT_ALIGN_CENTER)
            cursor_x += (rect->screen_width - line_width) * 0.5f;
        else if (text->alignment == UI_TEXT_ALIGN_RIGHT)
            cursor_x += rect->screen_width - line_width;

        for (const char* p = line_start; p < line_end; p++)
        {
            float x0, y0, x1, y1, u0, v0, u1, v1;
            float xpos = cursor_x;
            float ypos = cursor_y;
            if (!Font_GetGlyphQuad(font, (unsigned char)*p, &xpos, &ypos, scale, &x0, &y0, &x1, &y1, &u0, &v0, &u1, &v1))
                continue;
            OverlayDrawList_AddQuad(list, x0, y0, x1, y0, x1, y1, x0, y1,
                u0, v0, u1, v0, u1, v1, u0, v1,
                r, g, b, a, texture, intersect_x, intersect_y, intersect_w, intersect_h);
            cursor_x = xpos;
        }

        if (*line_end == '\n')
            line_end++;
        else if (*line_end == ' ' && wrap_width > 0.0f)
            line_end++;

        str = line_end;
        cursor_y += line_height;
        if (cursor_y > rect->screen_y + rect->screen_height + line_height)
            break;
        (void)line_start;
    }
}





// Draws a UI tree
static void DrawUITree(Scene* scene, uint32_t entity_id, OverlayDrawList* list, float clip_x, float clip_y, float clip_w, float clip_h)
{
    if (entity_id == ENTITY_NONE || !scene->is_active_in_hierarchy[entity_id])
        return;
    if (!(scene->component_masks[entity_id] & COMPONENT_UI_RECT_TRANSFORM))
        return;

    EmitImage(scene, entity_id, list, clip_x, clip_y, clip_w, clip_h);
    EmitText(scene, entity_id, list, clip_x, clip_y, clip_w, clip_h);

    uint32_t child = scene->transforms[entity_id].first_child_id;
    while (child != ENTITY_NONE)
    {
        DrawUITree(scene, child, list, clip_x, clip_y, clip_w, clip_h);
        child = scene->transforms[child].next_sibling_id;
    }
}





// Builds the UI overlay
void Scene_BuildUIOverlay(Scene* scene, OverlayDrawList* out_list)
{
    if (!scene || !out_list)
        return;

    OverlayDrawList_Reset(out_list);

    UICanvasSortEntry canvases[MAX_UI_CANVASES];
    uint32_t canvas_count = GatherCanvases(scene, canvases, MAX_UI_CANVASES);
    qsort(canvases, canvas_count, sizeof(UICanvasSortEntry), CompareCanvasDrawOrder);

    for (uint32_t i = 0; i < canvas_count; i++)
    {
        uint32_t id = canvases[i].entity_id;
        RectTransformComponent* rect = &scene->rect_transforms[id];
        DrawUITree(scene, id, out_list, rect->screen_x, rect->screen_y, rect->screen_width, rect->screen_height);
    }
}