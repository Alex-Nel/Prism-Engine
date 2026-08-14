#ifndef SCENE_UI_H
#define SCENE_UI_H

#include <stdbool.h>
#include <stdint.h>

#include "scene_structures.h"


typedef enum UICanvasScaleMode
{
    UI_CANVAS_CONSTANT_PIXEL_SIZE = 0,
    UI_CANVAS_SCALE_WITH_SCREEN_SIZE = 1
} UICanvasScaleMode;


typedef enum UITextAlignment
{
    UI_TEXT_ALIGN_LEFT,
    UI_TEXT_ALIGN_CENTER,
    UI_TEXT_ALIGN_RIGHT
} UITextAlignment;


typedef enum UIButtonState
{
    UI_BUTTON_STATE_NORMAL,
    UI_BUTTON_STATE_HOVERED,
    UI_BUTTON_STATE_PRESSED,
    UI_BUTTON_STATE_DISABLED
} UIButtonState;


typedef enum UIPointerEvent
{
    UI_POINTER_ENTER,
    UI_POINTER_EXIT,
    UI_POINTER_DOWN,
    UI_POINTER_UP,
    UI_POINTER_CLICK
} UIPointerEvent;


typedef struct UICanvasComponent
{
    Entity entity;
    bool is_active;

    int sort_order;
    UICanvasScaleMode scale_mode;
    Vector2 reference_resolution;
    float match_width_or_height;
    bool blocks_raycasts;

    float scale_factor;
} UICanvasComponent;


typedef struct RectTransformComponent
{
    Entity entity;

    Vector2 anchor_min;
    Vector2 anchor_max;
    Vector2 pivot;
    Vector2 size_delta;
    Vector2 anchored_position;

    float screen_x;
    float screen_y;
    float screen_width;
    float screen_height;

    bool is_dirty;
} RectTransformComponent;


typedef struct UIImageComponent
{
    Entity entity;
    bool is_active;
    Texture* texture;
    Color color;
    bool raycast_target;
} UIImageComponent;


typedef struct UITextComponent
{
    Entity entity;
    bool is_active;
    char text[256];
    Font* font;
    Color color;
    UITextAlignment alignment;
    float font_size;
    bool wrap;
    bool raycast_target;
} UITextComponent;


typedef struct UIButtonComponent
{
    Entity entity;
    bool is_active;
    bool interactable;

    UIButtonState current_state;
    Color color_normal;
    Color color_hovered;
    Color color_pressed;
    Color color_disabled;

    bool clicked_this_frame;
} UIButtonComponent;


struct Renderer;

void RetainedUI_Reset(Scene* scene);
void RetainedUI_Shutdown(Scene* scene);
void RetainedUI_PreUpdate(Scene* scene, uint32_t window_w, uint32_t window_h,
    float mouse_x, float mouse_y, bool mouse_captured);
void RetainedUI_Render(Scene* scene, struct Renderer* renderer, uint32_t window_w, uint32_t window_h);

void UICanvas_SetActive(Entity entity, bool active);
void UICanvas_SetScaleMode(Entity entity, UICanvasScaleMode mode);
void UICanvas_SetReferenceResolution(Entity entity, Vector2 resolution);
void UICanvas_SetMatchWidthOrHeight(Entity entity, float match);

void RectTransform_MarkDirty(Entity entity);
void RectTransform_SetAnchoredPosition(Entity entity, Vector2 position);
void RectTransform_SetSizeDelta(Entity entity, Vector2 size);
void RectTransform_SetAnchors(Entity entity, Vector2 min, Vector2 max);
void RectTransform_SetPivot(Entity entity, Vector2 pivot);
void UIText_SetText(Entity entity, const char* text);


#endif
