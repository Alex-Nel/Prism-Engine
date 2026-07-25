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



static struct nk_context ctx;
static struct nk_font_atlas atlas;
static struct nk_color current_theme_table[NK_COLOR_COUNT];



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





bool UI_BeginWindow(const char* title, float x, float y, float width, float height, int flags)
{
    return nk_begin(&ctx, title, nk_rect(x, y, width, height), (nk_flags)flags);
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





bool UI_SliderInt(int min, int* val, int max, int step)
{
    return nk_slider_int(&ctx, min, val, max, step);
}





bool UI_Checkbox(const char* label, bool* active)
{
    int is_active = *active ? 1 : 0;
    bool clicked = nk_checkbox_label(&ctx, label, &is_active);
    *active = is_active != 0;
    return clicked;
}





bool UI_RadioButton(const char* label, bool active)
{
    return nk_option_label(&ctx, label, active ? 1 : 0);
}





void UI_PropertyInt(const char* name, int min, int* val, int max, int step, float inc_per_pixel)
{
    nk_property_int(&ctx, name, min, val, max, step, inc_per_pixel);
}





bool UI_ColorPicker(Color* color)
{
    struct nk_colorf col = { color->r, color->g, color->b, color->a };
    bool changed = nk_color_pick(&ctx, &col, NK_RGBA);
    if (changed)
    {
        color->r = col.r;
        color->g = col.g;
        color->b = col.b;
        color->a = col.a;
    }
    return changed;
}





bool UI_Combo(const char** items, int count, int* selected, int item_height, float width, float height)
{
    return nk_combobox(&ctx, items, count, selected, item_height, nk_vec2(width, height));
}





void UI_SetTheme(int theme_id)
{
    if (theme_id == 1)
    {
        // Light
        current_theme_table[NK_COLOR_TEXT] = nk_rgba(70, 70, 70, 255);
        current_theme_table[NK_COLOR_WINDOW] = nk_rgba(175, 175, 175, 255);
        current_theme_table[NK_COLOR_HEADER] = nk_rgba(175, 175, 175, 255);
        current_theme_table[NK_COLOR_BORDER] = nk_rgba(0, 0, 0, 255);
        current_theme_table[NK_COLOR_BUTTON] = nk_rgba(185, 185, 185, 255);
        current_theme_table[NK_COLOR_BUTTON_HOVER] = nk_rgba(215, 215, 215, 255);
        current_theme_table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(235, 235, 235, 255);
        current_theme_table[NK_COLOR_TOGGLE] = nk_rgba(150, 150, 150, 255);
        current_theme_table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(120, 120, 120, 255);
        current_theme_table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(175, 175, 255, 255);
        current_theme_table[NK_COLOR_SELECT] = nk_rgba(190, 190, 190, 255);
        current_theme_table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(175, 175, 175, 255);
        current_theme_table[NK_COLOR_SLIDER] = nk_rgba(190, 190, 190, 255);
        current_theme_table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(80, 80, 80, 255);
        current_theme_table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(70, 70, 70, 255);
        current_theme_table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(60, 60, 60, 255);
        current_theme_table[NK_COLOR_PROPERTY] = nk_rgba(175, 175, 175, 255);
        current_theme_table[NK_COLOR_EDIT] = nk_rgba(150, 150, 150, 255);
        current_theme_table[NK_COLOR_EDIT_CURSOR] = nk_rgba(0, 0, 0, 255);
        current_theme_table[NK_COLOR_COMBO] = nk_rgba(175, 175, 175, 255);
        current_theme_table[NK_COLOR_CHART] = nk_rgba(160, 160, 160, 255);
        current_theme_table[NK_COLOR_CHART_COLOR] = nk_rgba(45, 45, 45, 255);
        current_theme_table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba( 255, 0, 0, 255);
        current_theme_table[NK_COLOR_SCROLLBAR] = nk_rgba(180, 180, 180, 255);
        current_theme_table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(140, 140, 140, 255);
        current_theme_table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(150, 150, 150, 255);
        current_theme_table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(160, 160, 160, 255);
        current_theme_table[NK_COLOR_TAB_HEADER] = nk_rgba(180, 180, 180, 255);
        nk_style_from_table(&ctx, current_theme_table);
    }
    else if (theme_id == 2)
    {
        // Red
        current_theme_table[NK_COLOR_TEXT] = nk_rgba(190, 190, 190, 255);
        current_theme_table[NK_COLOR_WINDOW] = nk_rgba(30, 33, 40, 215);
        current_theme_table[NK_COLOR_HEADER] = nk_rgba(181, 45, 69, 220);
        current_theme_table[NK_COLOR_BORDER] = nk_rgba(51, 55, 67, 255);
        current_theme_table[NK_COLOR_BUTTON] = nk_rgba(181, 45, 69, 255);
        current_theme_table[NK_COLOR_BUTTON_HOVER] = nk_rgba(190, 50, 70, 255);
        current_theme_table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(195, 55, 75, 255);
        current_theme_table[NK_COLOR_TOGGLE] = nk_rgba(51, 55, 67, 255);
        current_theme_table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 60, 60, 255);
        current_theme_table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(181, 45, 69, 255);
        current_theme_table[NK_COLOR_SELECT] = nk_rgba(51, 55, 67, 255);
        current_theme_table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(181, 45, 69, 255);
        current_theme_table[NK_COLOR_SLIDER] = nk_rgba(51, 55, 67, 255);
        current_theme_table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(181, 45, 69, 255);
        current_theme_table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(186, 50, 74, 255);
        current_theme_table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(191, 55, 79, 255);
        current_theme_table[NK_COLOR_PROPERTY] = nk_rgba(51, 55, 67, 255);
        current_theme_table[NK_COLOR_EDIT] = nk_rgba(51, 55, 67, 225);
        current_theme_table[NK_COLOR_EDIT_CURSOR] = nk_rgba(190, 190, 190, 255);
        current_theme_table[NK_COLOR_COMBO] = nk_rgba(51, 55, 67, 255);
        current_theme_table[NK_COLOR_CHART] = nk_rgba(51, 55, 67, 255);
        current_theme_table[NK_COLOR_CHART_COLOR] = nk_rgba(170, 40, 60, 255);
        current_theme_table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba( 255, 0, 0, 255);
        current_theme_table[NK_COLOR_SCROLLBAR] = nk_rgba(30, 33, 40, 255);
        current_theme_table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(64, 84, 95, 255);
        current_theme_table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(70, 90, 100, 255);
        current_theme_table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(75, 95, 105, 255);
        current_theme_table[NK_COLOR_TAB_HEADER] = nk_rgba(181, 45, 69, 220);
        nk_style_from_table(&ctx, current_theme_table);
    }
    else if (theme_id == 3)
    {
        // Blue
        current_theme_table[NK_COLOR_TEXT] = nk_rgba(20, 20, 20, 255);
        current_theme_table[NK_COLOR_WINDOW] = nk_rgba(202, 212, 214, 215);
        current_theme_table[NK_COLOR_HEADER] = nk_rgba(137, 182, 224, 220);
        current_theme_table[NK_COLOR_BORDER] = nk_rgba(140, 159, 173, 255);
        current_theme_table[NK_COLOR_BUTTON] = nk_rgba(137, 182, 224, 255);
        current_theme_table[NK_COLOR_BUTTON_HOVER] = nk_rgba(142, 187, 229, 255);
        current_theme_table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(147, 192, 234, 255);
        current_theme_table[NK_COLOR_TOGGLE] = nk_rgba(177, 210, 210, 255);
        current_theme_table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(170, 200, 200, 255);
        current_theme_table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(137, 182, 224, 255);
        current_theme_table[NK_COLOR_SELECT] = nk_rgba(177, 210, 210, 255);
        current_theme_table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(137, 182, 224, 255);
        current_theme_table[NK_COLOR_SLIDER] = nk_rgba(177, 210, 210, 255);
        current_theme_table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(137, 182, 224, 245);
        current_theme_table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(142, 188, 229, 255);
        current_theme_table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(147, 193, 234, 255);
        current_theme_table[NK_COLOR_PROPERTY] = nk_rgba(210, 210, 210, 255);
        current_theme_table[NK_COLOR_EDIT] = nk_rgba(210, 210, 210, 225);
        current_theme_table[NK_COLOR_EDIT_CURSOR] = nk_rgba(20, 20, 20, 255);
        current_theme_table[NK_COLOR_COMBO] = nk_rgba(210, 210, 210, 255);
        current_theme_table[NK_COLOR_CHART] = nk_rgba(210, 210, 210, 255);
        current_theme_table[NK_COLOR_CHART_COLOR] = nk_rgba(137, 182, 224, 255);
        current_theme_table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba( 255, 0, 0, 255);
        current_theme_table[NK_COLOR_SCROLLBAR] = nk_rgba(190, 200, 200, 255);
        current_theme_table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(64, 84, 95, 255);
        current_theme_table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(70, 90, 100, 255);
        current_theme_table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(75, 95, 105, 255);
        current_theme_table[NK_COLOR_TAB_HEADER] = nk_rgba(137, 182, 224, 220);
        nk_style_from_table(&ctx, current_theme_table);
    }
    else
    {
        // Dark (Default)
        // Set the table to nuklear's default colors, so UI_SetStyleColor can still modify it
        nk_style_default(&ctx);
        const struct nk_color default_colors[NK_COLOR_COUNT] = {
            {175,175,175,255}, {45, 45, 45, 255}, {40, 40, 40, 255},
            {65, 65, 65, 255}, {50, 50, 50, 255}, {40, 40, 40, 255},
            {35, 35, 35, 255}, {100,100,100,255}, {120,120,120,255},
            {45, 45, 45, 255}, {45, 45, 45, 255}, {35, 35, 35, 255},
            {38, 38, 38, 255}, {100,100,100,255}, {120,120,120,255},
            {150,150,150,255}, {38, 38, 38, 255}, {38, 38, 38, 255},
            {175,175,175,255}, {45, 45, 45, 255}, {120,120,120,255},
            {45, 45, 45, 255}, { 255, 0, 0, 255}, {40, 40, 40, 255},
            {100,100,100,255}, {120,120,120,255}, {150,150,150,255},
            {40, 40, 40, 255}
        };

        for (int i = 0; i < NK_COLOR_COUNT; ++i)
            current_theme_table[i] = default_colors[i];
    }
}





void UI_SetElementStyleColor(int element_color_id, Color color)
{
    if (element_color_id >= 0 && element_color_id < NK_COLOR_COUNT)
    {
        current_theme_table[element_color_id] = nk_rgba_f(color.r, color.g, color.b, color.a);
        nk_style_from_table(&ctx, current_theme_table);
    }
}