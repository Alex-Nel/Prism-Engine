#define NK_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "../include/nuklear.h"

#include "ui_core.h"
#include <string.h>



static struct nk_context ctx;
static struct nk_font_atlas atlas;



void UI_Init()
{
    nk_init_default(&ctx, 0);
    // Font setup is deferred to the UI renderer
}





void UI_Shutdown()
{
    nk_free(&ctx);
    nk_font_atlas_clear(&atlas);
}





void UI_InputBegin()
{
    nk_input_begin(&ctx);
}





bool UI_ProcessEvent(Event* e)
{
    if (e->type == EVENT_MOUSE_MOVED)
    {
        nk_input_motion(&ctx, (int)e->mouse_state.x, (int)e->mouse_state.y);
    }
    else if (e->type == EVENT_MOUSE_BUTTON_PRESSED || e->type == EVENT_MOUSE_BUTTON_RELEASED)
    {
        int down = (e->type == EVENT_MOUSE_BUTTON_PRESSED) ? 1 : 0;
        int id = -1;
        if (e->mouse_button.button == MOUSE_BUTTON_LEFT)
            id = NK_BUTTON_LEFT;
        else if (e->mouse_button.button == MOUSE_BUTTON_RIGHT)
            id = NK_BUTTON_RIGHT;
        else if (e->mouse_button.button == MOUSE_BUTTON_MIDDLE)
            id = NK_BUTTON_MIDDLE;
        
        if (id != -1)
            nk_input_button(&ctx, (enum nk_buttons)id, (int)ctx.input.mouse.pos.x, (int)ctx.input.mouse.pos.y, down);
    }
    else if (e->type == EVENT_MOUSEWHEEL_SCROLLED)
    {
        struct nk_vec2 scroll;
        scroll.x = 0;
        scroll.y = e->mouse_scroll.delta_y;
        nk_input_scroll(&ctx, scroll);
    }
    else if (e->type == EVENT_KEY_PRESSED || e->type == EVENT_KEY_RELEASED)
    {
        int down = (e->type == EVENT_KEY_PRESSED) ? 1 : 0;
        
        if (e->key.key == KEYCODE_LEFTCTRL || e->key.key == KEYCODE_RIGHTCTRL)        nk_input_key(&ctx, NK_KEY_CTRL, down);
        else if (e->key.key == KEYCODE_LEFTSHIFT || e->key.key == KEYCODE_RIGHTSHIFT) nk_input_key(&ctx, NK_KEY_SHIFT, down);
        else if (e->key.key == KEYCODE_ENTER)      nk_input_key(&ctx, NK_KEY_ENTER, down);
        else if (e->key.key == KEYCODE_BACKSPACE)  nk_input_key(&ctx, NK_KEY_BACKSPACE, down);
        else if (e->key.key == KEYCODE_LEFTARROW)  nk_input_key(&ctx, NK_KEY_LEFT, down);
        else if (e->key.key == KEYCODE_RIGHTARROW) nk_input_key(&ctx, NK_KEY_RIGHT, down);
        else if (e->key.key == KEYCODE_UPARROW)    nk_input_key(&ctx, NK_KEY_UP, down);
        else if (e->key.key == KEYCODE_DOWNARROW)  nk_input_key(&ctx, NK_KEY_DOWN, down);
    }
    else if (e->type == EVENT_TEXT_INPUT)
    {
        nk_input_glyph(&ctx, e->text_input.text);
    }
    
    if (nk_item_is_any_active(&ctx) || nk_window_is_any_hovered(&ctx))
    {
        return true;
    }

    return false;
}





void UI_InputEnd()
{
    nk_input_end(&ctx);
}





struct nk_context* UI_GetContext()
{
    return &ctx;
}





bool UI_BeginWindow(const char* title, float x, float y, float width, float height)
{
    return nk_begin(&ctx, title, nk_rect(x, y, width, height), NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE);
}





void UI_EndWindow()
{
    nk_end(&ctx);
}





void UI_LayoutRowDynamic(float item_height, int cols)
{
    nk_layout_row_dynamic(&ctx, item_height, cols);
}





bool UI_Button(const char* label)
{
    return nk_button_label(&ctx, label);
}





void UI_Label(const char* text)
{
    nk_label(&ctx, text, NK_TEXT_LEFT);
}





bool UI_SliderFloat(float min, float* val, float max, float step)
{
    return nk_slider_float(&ctx, min, val, max, step);
}