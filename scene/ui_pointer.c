#include "../core/input_core.h"
#include "scene.h"
#include <stdlib.h>





// Compares the hit order of two canvases
static int CompareCanvasHitOrder(const void* a, const void* b)
{
    const UICanvasSortEntry* left = (const UICanvasSortEntry*)a;
    const UICanvasSortEntry* right = (const UICanvasSortEntry*)b;
    if (left->sort_order < right->sort_order) return 1;
    if (left->sort_order > right->sort_order) return -1;
    return 0;
}





// Determines if a point is within a rect transform
static bool PointInRect(const RectTransformComponent* rect, float x, float y)
{
    return x >= rect->screen_x && y >= rect->screen_y && x <= rect->screen_x + rect->screen_width && y <= rect->screen_y + rect->screen_height;
}





// Determines if an entity blocks a raycast
static bool EntityBlocksRaycast(Scene* scene, uint32_t id)
{
    if (!scene->is_active_in_hierarchy[id] || !(scene->component_masks[id] & COMPONENT_UI_RECT_TRANSFORM))
    {
        return false;
    }

    if (scene->component_masks[id] & COMPONENT_UI_BUTTON)
    {
        UIButtonComponent* button = &scene->ui_buttons[id];
        return button->is_active && button->interactable;
    }
    
    if ((scene->component_masks[id] & COMPONENT_UI_IMAGE) && scene->ui_images[id].is_active && scene->ui_images[id].raycast_target)
    {
        return true;
    }
    
    if ((scene->component_masks[id] & COMPONENT_UI_TEXT) && scene->ui_texts[id].is_active && scene->ui_texts[id].raycast_target)
    {
        return true;
    }

    return false;
}





// Recursively sees whether a mouse hits a tree of UI components
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
        PointInRect(&scene->ui_rect_transforms[entity_id], mouse_x, mouse_y) &&
        EntityBlocksRaycast(scene, entity_id))
    {
        return entity_id;
    }

    return ENTITY_NONE;
}





// Emits a pointer event for any scripts
static void FirePointer(Scene* scene, uint32_t entity_id, UIPointerEvent event)
{
    if (entity_id == ENTITY_NONE || entity_id >= MAX_ENTITIES || scene->component_masks[entity_id] == COMPONENT_NONE || !(scene->component_masks[entity_id] & COMPONENT_SCRIPT))
        return;

    ScriptComponent* scripts = &scene->scripts[entity_id];
    Entity self = { entity_id, scene };
    for (uint32_t i = 0; i < scripts->count; i++)
    {
        ScriptInstance* script = &scripts->instances[i];
        if (!script->is_active)
            continue;

        PointerCallback callback = NULL;
        switch (event)
        {
            case UI_POINTER_ENTER: callback = script->OnPointerEnter; break;
            case UI_POINTER_EXIT: callback = script->OnPointerExit; break;
            case UI_POINTER_DOWN: callback = script->OnPointerDown; break;
            case UI_POINTER_UP: callback = script->OnPointerUp; break;
            case UI_POINTER_CLICK: callback = script->OnPointerClick; break;
        }

        if (callback)
            callback(self, script->instance_data);
    }
}





// Updates the state of a UI button
static void UpdateButtonState(Scene* scene, uint32_t id, bool hovered, bool pressed)
{
    UIButtonComponent* button = &scene->ui_buttons[id];
    if (!button->is_active || !button->interactable)
        button->current_state = UI_BUTTON_STATE_DISABLED;
    else if (pressed)
        button->current_state = UI_BUTTON_STATE_PRESSED;
    else if (hovered)
        button->current_state = UI_BUTTON_STATE_HOVERED;
    else
        button->current_state = UI_BUTTON_STATE_NORMAL;
}





// Returns whether an entity is an active UI entity
static bool IsLiveUIEntity(Scene* scene, uint32_t id)
{
    return id != ENTITY_NONE && id < MAX_ENTITIES && scene->component_masks[id] != COMPONENT_NONE && scene->is_active_in_hierarchy[id];
}





// Processes pointer events for all UI elements
void RetainedUI_ProcessPointer(Scene* scene, float mouse_x, float mouse_y, bool mouse_captured)
{
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (scene->component_masks[i] & COMPONENT_UI_BUTTON)
            scene->ui_buttons[i].clicked_this_frame = false;
    }

    if (!IsLiveUIEntity(scene, g_ui_state.hovered_entity_id))
        g_ui_state.hovered_entity_id = ENTITY_NONE;
    if (!IsLiveUIEntity(scene, g_ui_state.pressed_entity_id))
        g_ui_state.pressed_entity_id = ENTITY_NONE;

    if (mouse_captured)
    {
        FirePointer(scene, g_ui_state.hovered_entity_id, UI_POINTER_EXIT);
        g_ui_state.hovered_entity_id = ENTITY_NONE;
        g_ui_state.pressed_entity_id = ENTITY_NONE;
        g_ui_state.blocks_pointer = false;
        return;
    }

    uint32_t canvas_count = RetainedUI_GatherCanvases(scene);
    qsort(g_ui_state.canvas_entries, canvas_count, sizeof(UICanvasSortEntry), CompareCanvasHitOrder);

    uint32_t hit = ENTITY_NONE;
    for (uint32_t i = 0; i < canvas_count; i++)
    {
        uint32_t canvas_id = g_ui_state.canvas_entries[i].entity_id;
        if (!scene->ui_canvases[canvas_id].blocks_raycasts)
            continue;

        hit = HitTestTree(scene, canvas_id, mouse_x, mouse_y);
        if (hit != ENTITY_NONE)
            break;
    }

    if (hit != g_ui_state.hovered_entity_id)
    {
        FirePointer(scene, g_ui_state.hovered_entity_id, UI_POINTER_EXIT);
        FirePointer(scene, hit, UI_POINTER_ENTER);
        g_ui_state.hovered_entity_id = hit;
    }

    bool pressed_this_frame = Input_IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool down = Input_IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool released = Input_IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    bool owned_release = released && g_ui_state.pressed_entity_id != ENTITY_NONE;

    if (pressed_this_frame && hit != ENTITY_NONE)
    {
        g_ui_state.pressed_entity_id = hit;
        FirePointer(scene, hit, UI_POINTER_DOWN);
    }

    if (released && g_ui_state.pressed_entity_id != ENTITY_NONE)
    {
        uint32_t pressed_id = g_ui_state.pressed_entity_id;
        FirePointer(scene, pressed_id, UI_POINTER_UP);
        if (pressed_id == hit)
        {
            if (scene->component_masks[hit] & COMPONENT_UI_BUTTON)
            {
                UIButtonComponent* button = &scene->ui_buttons[hit];
                if (button->is_active && button->interactable)
                    button->clicked_this_frame = true;
            }
            FirePointer(scene, hit, UI_POINTER_CLICK);
        }
        g_ui_state.pressed_entity_id = ENTITY_NONE;
    }

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!(scene->component_masks[i] & COMPONENT_UI_BUTTON))
            continue;
        
        UpdateButtonState(scene, i, i == hit, i == g_ui_state.pressed_entity_id && down);
    }

    g_ui_state.blocks_pointer = hit != ENTITY_NONE || g_ui_state.pressed_entity_id != ENTITY_NONE || owned_release;
}