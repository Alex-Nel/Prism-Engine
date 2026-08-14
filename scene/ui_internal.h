#ifndef SCENE_UI_INTERNAL_H
#define SCENE_UI_INTERNAL_H


#include "scene.h"
#include "../core/overlay_core.h"



typedef struct UICanvasSortEntry
{
    uint32_t entity_id;
    int sort_order;
} UICanvasSortEntry;



struct RetainedUIContext
{
    UICanvasComponent* canvases;
    RectTransformComponent* rect_transforms;
    UIImageComponent* images;
    UITextComponent* texts;
    UIButtonComponent* buttons;

    OverlayDrawList draw_list;
    UICanvasSortEntry* canvas_entries;
    uint32_t canvas_count;
    uint32_t canvas_capacity;

    uint32_t hovered_entity_id;
    uint32_t pressed_entity_id;
    uint32_t window_width;
    uint32_t window_height;
    bool blocks_pointer;
    bool layout_dirty;
};



RetainedUIContext* RetainedUI_EnsureContext(Scene* scene);
uint32_t RetainedUI_GatherCanvases(Scene* scene);
void RetainedUI_UpdateLayoutInternal(Scene* scene, uint32_t window_w, uint32_t window_h);
void RetainedUI_ProcessPointerInternal(Scene* scene, float mouse_x, float mouse_y, bool mouse_captured);
void RetainedUI_BuildOverlayInternal(Scene* scene);



#endif