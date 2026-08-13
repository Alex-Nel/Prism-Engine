#ifndef OVERLAY_CORE_H
#define OVERLAY_CORE_H


#include <stdint.h>
#include <stdbool.h>
#include "mesh_core.h"



// Vertex layout matches Nuklear's nk_draw_vertex so the UI shader/VAO can be reused.
typedef struct OverlayVertex
{
    float position[2];
    float uv[2];
    uint8_t color[4];
} OverlayVertex;



// A command to draw an overlay
typedef struct OverlayDrawCmd
{
    uint32_t index_offset;
    uint32_t index_count;
    TextureHandle texture;
    float clip_x;
    float clip_y;
    float clip_w;
    float clip_h;
} OverlayDrawCmd;



// A list of overlay draw commands
typedef struct OverlayDrawList
{
    OverlayVertex* vertices;
    uint32_t vertex_count;
    uint32_t vertex_capacity;

    uint16_t* indices;
    uint32_t index_count;
    uint32_t index_capacity;

    OverlayDrawCmd* commands;
    uint32_t command_count;
    uint32_t command_capacity;
} OverlayDrawList;



void OverlayDrawList_Init(OverlayDrawList* list);
void OverlayDrawList_Reset(OverlayDrawList* list);
void OverlayDrawList_Free(OverlayDrawList* list);

void OverlayDrawList_AddRect(OverlayDrawList* list,
    float x, float y, float w, float h,
    float u0, float v0, float u1, float v1,
    uint8_t r, uint8_t g, uint8_t b, uint8_t a,
    TextureHandle texture,
    float clip_x, float clip_y, float clip_w, float clip_h);

void OverlayDrawList_AddQuad(OverlayDrawList* list,
    float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
    float u0, float v0, float u1, float v1, float u2, float v2, float u3, float v3,
    uint8_t r, uint8_t g, uint8_t b, uint8_t a,
    TextureHandle texture,
    float clip_x, float clip_y, float clip_w, float clip_h);



#endif