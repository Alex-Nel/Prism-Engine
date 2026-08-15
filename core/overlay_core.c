#include "overlay_core.h"
#include "log_core.h"
#include <stdlib.h>
#include <string.h>



static bool OverlayDrawList_ReserveVertices(OverlayDrawList* list, uint32_t extra)
{
    uint32_t needed = list->vertex_count + extra;
    if (needed <= list->vertex_capacity)
        return true;

    uint32_t capacity = list->vertex_capacity ? list->vertex_capacity : 256;
    while (capacity < needed)
        capacity *= 2;

    if (capacity > 65535)
        capacity = 65535;
    if (needed > capacity)
        return false;

    OverlayVertex* vertices = (OverlayVertex*)realloc(list->vertices, capacity * sizeof(OverlayVertex));
    if (!vertices)
        return false;

    list->vertices = vertices;
    list->vertex_capacity = capacity;
    return true;
}





static bool OverlayDrawList_ReserveIndices(OverlayDrawList* list, uint32_t extra)
{
    uint32_t needed = list->index_count + extra;
    if (needed <= list->index_capacity)
        return true;

    uint32_t capacity = list->index_capacity ? list->index_capacity : 512;
    while (capacity < needed)
        capacity *= 2;

    uint16_t* indices = (uint16_t*)realloc(list->indices, capacity * sizeof(uint16_t));
    if (!indices)
        return false;

    list->indices = indices;
    list->index_capacity = capacity;
    return true;
}





static bool OverlayDrawList_ReserveCommands(OverlayDrawList* list, uint32_t extra)
{
    uint32_t needed = list->command_count + extra;
    if (needed <= list->command_capacity)
        return true;

    uint32_t capacity = list->command_capacity ? list->command_capacity : 32;
    while (capacity < needed)
        capacity *= 2;

    OverlayDrawCmd* commands = (OverlayDrawCmd*)realloc(list->commands, capacity * sizeof(OverlayDrawCmd));
    if (!commands)
        return false;

    list->commands = commands;
    list->command_capacity = capacity;
    return true;
}





static bool OverlayClipEqual(const OverlayDrawCmd* cmd, float clip_x, float clip_y, float clip_w, float clip_h)
{
    return cmd->clip_x == clip_x && cmd->clip_y == clip_y && cmd->clip_w == clip_w && cmd->clip_h == clip_h;
}





static OverlayDrawCmd* OverlayDrawList_BeginBatch(OverlayDrawList* list, TextureHandle texture, float clip_x, float clip_y, float clip_w, float clip_h)
{
    if (list->command_count > 0)
    {
        OverlayDrawCmd* last = &list->commands[list->command_count - 1];
        if (last->texture.id == texture.id && OverlayClipEqual(last, clip_x, clip_y, clip_w, clip_h))
            return last;
    }

    if (!OverlayDrawList_ReserveCommands(list, 1))
        return NULL;

    OverlayDrawCmd* cmd = &list->commands[list->command_count++];
    cmd->index_offset = list->index_count;
    cmd->index_count = 0;
    cmd->texture = texture;
    cmd->clip_x = clip_x;
    cmd->clip_y = clip_y;
    cmd->clip_w = clip_w;
    cmd->clip_h = clip_h;
    return cmd;
}





void OverlayDrawList_Init(OverlayDrawList* list)
{
    if (!list)
        return;
    memset(list, 0, sizeof(OverlayDrawList));
}





void OverlayDrawList_Reset(OverlayDrawList* list)
{
    if (!list)
        return;
    list->vertex_count = 0;
    list->index_count = 0;
    list->command_count = 0;
}





void OverlayDrawList_Free(OverlayDrawList* list)
{
    if (!list)
        return;
    free(list->vertices);
    free(list->indices);
    free(list->commands);
    memset(list, 0, sizeof(OverlayDrawList));
}





void OverlayDrawList_AddQuad(OverlayDrawList* list,
    float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
    float u0, float v0, float u1, float v1, float u2, float v2, float u3, float v3,
    uint8_t r, uint8_t g, uint8_t b, uint8_t a,
    TextureHandle texture,
    float clip_x, float clip_y, float clip_w, float clip_h)
{
    if (!list)
        return;

    if (!OverlayDrawList_ReserveVertices(list, 4) || !OverlayDrawList_ReserveIndices(list, 6))
    {
        Log_Warning("WARNING: Overlay draw list is full");
        return;
    }

    OverlayDrawCmd* cmd = OverlayDrawList_BeginBatch(list, texture, clip_x, clip_y, clip_w, clip_h);
    if (!cmd)
        return;

    uint16_t base = (uint16_t)list->vertex_count;

    OverlayVertex verts[4] = {
        { {x0, y0}, {u0, v0}, {r, g, b, a} },
        { {x1, y1}, {u1, v1}, {r, g, b, a} },
        { {x2, y2}, {u2, v2}, {r, g, b, a} },
        { {x3, y3}, {u3, v3}, {r, g, b, a} }
    };

    memcpy(&list->vertices[list->vertex_count], verts, sizeof(verts));
    list->vertex_count += 4;

    list->indices[list->index_count + 0] = base + 0;
    list->indices[list->index_count + 1] = base + 1;
    list->indices[list->index_count + 2] = base + 2;
    list->indices[list->index_count + 3] = base + 0;
    list->indices[list->index_count + 4] = base + 2;
    list->indices[list->index_count + 5] = base + 3;
    list->index_count += 6;
    cmd->index_count += 6;
}





void OverlayDrawList_AddRect(OverlayDrawList* list,
    float x, float y, float w, float h,
    float u0, float v0, float u1, float v1,
    uint8_t r, uint8_t g, uint8_t b, uint8_t a,
    TextureHandle texture,
    float clip_x, float clip_y, float clip_w, float clip_h)
{
    OverlayDrawList_AddQuad(list,
        x, y,
        x + w, y,
        x + w, y + h,
        x, y + h,
        u0, v0, u1, v0, u1, v1, u0, v1,
        r, g, b, a, texture, clip_x, clip_y, clip_w, clip_h);
}