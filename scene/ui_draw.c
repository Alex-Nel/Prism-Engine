#include "scene.h"
#include <stdlib.h>





// Converts a color channel into a single byte
static uint8_t ColorChannelToByte(float channel)
{
    if (channel < 0.0f) channel = 0.0f;
    if (channel > 1.0f) channel = 1.0f;

    return (uint8_t)(channel * 255.0f + 0.5f);
}





// Multiplies two colors
static Color ColorMultiply(Color left, Color right)
{
    return (Color){
        left.r * right.r,
        left.g * right.g,
        left.b * right.b,
        left.a * right.a
    };
}





// Compares the draw order of two canvases
static int CompareCanvasDrawOrder(const void* a, const void* b)
{
    const UICanvasSortEntry* left = (const UICanvasSortEntry*)a;
    const UICanvasSortEntry* right = (const UICanvasSortEntry*)b;

    if (left->sort_order < right->sort_order) return -1;
    if (left->sort_order > right->sort_order) return 1;
    
    return 0;
}





// Emits an image to the overlay draw list
static void EmitImage(Scene* scene, uint32_t id, OverlayDrawList* list, float clip_x, float clip_y, float clip_w, float clip_h)
{
    if (!(scene->component_masks[id] & COMPONENT_UI_IMAGE))
        return;

    UIImageComponent* image = &scene->ui_images[id];
    if (!image->is_active)
        return;

    RectTransformComponent* rect = &scene->ui_rect_transforms[id];
    Color color = image->color;
    if (scene->component_masks[id] & COMPONENT_UI_BUTTON)
    {
        UIButtonComponent* button = &scene->ui_buttons[id];
        Color tint = button->color_normal;
        if (!button->is_active || !button->interactable)
            tint = button->color_disabled;
        else if (button->current_state == UI_BUTTON_STATE_PRESSED)
            tint = button->color_pressed;
        else if (button->current_state == UI_BUTTON_STATE_HOVERED)
            tint = button->color_hovered;
        color = ColorMultiply(color, tint);
    }

    TextureHandle texture = image->texture ? image->texture->gpu_handle : (TextureHandle){0};
    OverlayDrawList_AddRect(list,
        rect->screen_x, rect->screen_y, rect->screen_width, rect->screen_height,
        0.0f, 0.0f, 1.0f, 1.0f,
        ColorChannelToByte(color.r), ColorChannelToByte(color.g),
        ColorChannelToByte(color.b), ColorChannelToByte(color.a),
        texture, clip_x, clip_y, clip_w, clip_h);
}





// Advances the point in the font glyph
static float GlyphAdvance(const Font* font, unsigned char codepoint, float scale)
{
    float x = 0.0f, y = 0.0f, x0, y0, x1, y1, u0, v0, u1, v1;
    if (!Font_GetGlyphQuad(font, codepoint, &x, &y, scale, &x0, &y0, &x1, &y1, &u0, &v0, &u1, &v1))
        return 0.0f;

    return x;
}





// Emits a text to the overlay draw list
static void EmitText(Scene* scene, uint32_t id, OverlayDrawList* list, float clip_x, float clip_y, float clip_w, float clip_h)
{
    if (!(scene->component_masks[id] & COMPONENT_UI_TEXT))
        return;

    UITextComponent* text = &scene->ui_texts[id];
    if (!text->is_active || !text->font || !text->font->texture_atlas || text->text[0] == '\0')
        return;

    RectTransformComponent* rect = &scene->ui_rect_transforms[id];
    Font* font = text->font;
    float scale = font->size > 0.0f ? text->font_size / font->size : 1.0f;
    float wrap_width = text->wrap ? rect->screen_width : 0.0f;
    float line_height = font->line_height * scale;
    if (line_height <= 0.0f)
        line_height = text->font_size;

    uint8_t r = ColorChannelToByte(text->color.r);
    uint8_t g = ColorChannelToByte(text->color.g);
    uint8_t b = ColorChannelToByte(text->color.b);
    uint8_t a = ColorChannelToByte(text->color.a);
    TextureHandle texture = font->texture_atlas->gpu_handle;

    float intersect_x = rect->screen_x > clip_x ? rect->screen_x : clip_x;
    float intersect_y = rect->screen_y > clip_y ? rect->screen_y : clip_y;
    float intersect_r = (rect->screen_x + rect->screen_width) < (clip_x + clip_w)
        ? rect->screen_x + rect->screen_width : clip_x + clip_w;
    float intersect_b = (rect->screen_y + rect->screen_height) < (clip_y + clip_h)
        ? rect->screen_y + rect->screen_height : clip_y + clip_h;
    float intersect_w = intersect_r - intersect_x;
    float intersect_h = intersect_b - intersect_y;
    if (intersect_w <= 0.0f || intersect_h <= 0.0f)
        return;

    const char* str = text->text;
    float cursor_y = rect->screen_y + font->ascent * scale;
    while (*str)
    {
        const char* line_start = str;
        const char* line_end = str;
        float line_width = 0.0f;

        while (*line_end && *line_end != '\n')
        {
            if (wrap_width <= 0.0f)
            {
                line_width += GlyphAdvance(font, (unsigned char)*line_end, scale);
                line_end++;
                continue;
            }

            if (*line_end == ' ')
            {
                float space_width = GlyphAdvance(font, ' ', scale);
                if (line_width + space_width > wrap_width && line_width > 0.0f)
                    break;
                line_width += space_width;
                line_end++;
                continue;
            }

            const char* word_end = line_end;
            float word_width = 0.0f;
            while (*word_end && *word_end != '\n' && *word_end != ' ')
            {
                word_width += GlyphAdvance(font, (unsigned char)*word_end, scale);
                word_end++;
            }

            if (line_width + word_width > wrap_width && line_width > 0.0f)
                break;

            line_width += word_width;
            line_end = word_end;
        }

        float cursor_x = rect->screen_x;
        if (text->alignment == UI_TEXT_ALIGN_CENTER)
            cursor_x += (rect->screen_width - line_width) * 0.5f;
        else if (text->alignment == UI_TEXT_ALIGN_RIGHT)
            cursor_x += rect->screen_width - line_width;

        for (const char* character = line_start; character < line_end; character++)
        {
            float x0, y0, x1, y1, u0, v0, u1, v1;
            float xpos = cursor_x;
            float ypos = cursor_y;
            if (!Font_GetGlyphQuad(font, (unsigned char)*character, &xpos, &ypos, scale,
                &x0, &y0, &x1, &y1, &u0, &v0, &u1, &v1))
            {
                continue;
            }

            OverlayDrawList_AddQuad(list, x0, y0, x1, y0, x1, y1, x0, y1,
                u0, v0, u1, v0, u1, v1, u0, v1,
                r, g, b, a, texture, intersect_x, intersect_y, intersect_w, intersect_h);
            cursor_x = xpos;
        }

        if (*line_end == '\n')
            line_end++;
        else if (line_end == str)
            line_end++;

        str = line_end;
        cursor_y += line_height;
        
        if (cursor_y > rect->screen_y + rect->screen_height + line_height)
            break;
    }
}





// Draws an entire UI tree to an overlay draw list
static void DrawUITree(Scene* scene, uint32_t entity_id, OverlayDrawList* list, float clip_x, float clip_y, float clip_w, float clip_h)
{
    if (entity_id == ENTITY_NONE || !scene->is_active_in_hierarchy[entity_id])
        return;

    if (scene->component_masks[entity_id] & COMPONENT_UI_RECT_TRANSFORM)
    {
        EmitImage(scene, entity_id, list, clip_x, clip_y, clip_w, clip_h);
        EmitText(scene, entity_id, list, clip_x, clip_y, clip_w, clip_h);
    }

    uint32_t child = scene->transforms[entity_id].first_child_id;
    while (child != ENTITY_NONE)
    {
        DrawUITree(scene, child, list, clip_x, clip_y, clip_w, clip_h);
        child = scene->transforms[child].next_sibling_id;
    }
}





// Builds the overlay for every UI element in a scene
void RetainedUI_BuildOverlay(Scene* scene)
{
    OverlayDrawList_Reset(&g_ui_state.draw_list);

    uint32_t canvas_count = RetainedUI_GatherCanvases(scene);
    qsort(g_ui_state.canvas_entries, canvas_count, sizeof(UICanvasSortEntry), CompareCanvasDrawOrder);

    for (uint32_t i = 0; i < canvas_count; i++)
    {
        uint32_t id = g_ui_state.canvas_entries[i].entity_id;
        RectTransformComponent* rect = &scene->ui_rect_transforms[id];
        DrawUITree(scene, id, &g_ui_state.draw_list, rect->screen_x, rect->screen_y, rect->screen_width, rect->screen_height);
    }
}