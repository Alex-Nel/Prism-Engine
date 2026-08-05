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

#include <stdlib.h>
#include <string.h>



static struct nk_context ctx;
static struct nk_color current_theme_table[NK_COLOR_COUNT];

static UIClipboardSetCallback clipboard_set_text;
static UIClipboardGetCallback clipboard_get_text;
static UIClipboardFreeCallback clipboard_free_text;





// Pastes UTF-8 clipboard text into the active Nuklear editor
static void UI_ClipboardPaste(nk_handle userdata, struct nk_text_edit* edit)
{
    (void)userdata;

    if (!clipboard_get_text || !clipboard_free_text)
        return;

    char* text = clipboard_get_text();
    if (!text)
        return;

    nk_textedit_paste(edit, text, nk_strlen(text));
    clipboard_free_text(text);
}





// Copies a non-null-terminated Nuklear selection to the system clipboard
static void UI_ClipboardCopy(nk_handle userdata, const char* text, int len)
{
    (void)userdata;

    if (!clipboard_set_text || !text || len <= 0)
        return;

    char* terminated_text = (char*)malloc((size_t)len + 1);
    if (!terminated_text)
        return;

    memcpy(terminated_text, text, (size_t)len);
    terminated_text[len] = '\0';
    clipboard_set_text(terminated_text);
    free(terminated_text);
}





// Initializes the UI system (Nuklear and UI rendering)
void UI_Init()
{
    nk_init_default(&ctx, 0);
    ctx.clip.copy = UI_ClipboardCopy;
    ctx.clip.paste = UI_ClipboardPaste;
    // Font setup is deferred to the UI renderer
}





// Shuts down the UI system
void UI_Shutdown()
{
    nk_free(&ctx);
}





// Starts the input loop for the UI
void UI_InputBegin()
{
    nk_input_begin(&ctx);
}





// Processes engine events to see if they relate to any active UI
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
        
        // if (e->key.key == KEYCODE_LEFTCTRL || e->key.key == KEYCODE_RIGHTCTRL)        nk_input_key(&ctx, NK_KEY_CTRL, down);
        // else if (e->key.key == KEYCODE_LEFTSHIFT || e->key.key == KEYCODE_RIGHTSHIFT) nk_input_key(&ctx, NK_KEY_SHIFT, down);
        if (e->key.key == KEYCODE_LEFTCTRL || e->key.key == KEYCODE_RIGHTCTRL)         nk_input_key(&ctx, NK_KEY_CTRL, down);
        else if (e->key.key == KEYCODE_LEFTSHIFT || e->key.key == KEYCODE_RIGHTSHIFT)  nk_input_key(&ctx, NK_KEY_SHIFT, down);
        else if (e->key.key == KEYCODE_LEFTALT || e->key.key == KEYCODE_RIGHTALT)       nk_input_key(&ctx, NK_KEY_ALT, down);
        else if (e->key.key == KEYCODE_DELETE)     nk_input_key(&ctx, NK_KEY_DEL, down);
        else if (e->key.key == KEYCODE_ENTER)      nk_input_key(&ctx, NK_KEY_ENTER, down);
        else if (e->key.key == KEYCODE_TAB)        nk_input_key(&ctx, NK_KEY_TAB, down);
        else if (e->key.key == KEYCODE_BACKSPACE)  nk_input_key(&ctx, NK_KEY_BACKSPACE, down);
        else if (e->key.key == KEYCODE_LEFTARROW)  nk_input_key(&ctx, NK_KEY_LEFT, down);
        else if (e->key.key == KEYCODE_RIGHTARROW) nk_input_key(&ctx, NK_KEY_RIGHT, down);
        else if (e->key.key == KEYCODE_UPARROW)    nk_input_key(&ctx, NK_KEY_UP, down);
        else if (e->key.key == KEYCODE_DOWNARROW)  nk_input_key(&ctx, NK_KEY_DOWN, down);
        else if (e->key.key == KEYCODE_C)          nk_input_key(&ctx, NK_KEY_COPY, down && ctx.input.keyboard.keys[NK_KEY_CTRL].down);
        else if (e->key.key == KEYCODE_V)          nk_input_key(&ctx, NK_KEY_PASTE, down && ctx.input.keyboard.keys[NK_KEY_CTRL].down);
        else if (e->key.key == KEYCODE_X)          nk_input_key(&ctx, NK_KEY_CUT, down && ctx.input.keyboard.keys[NK_KEY_CTRL].down);
        else if (e->key.key == KEYCODE_A)          nk_input_key(&ctx, NK_KEY_TEXT_SELECT_ALL, down && ctx.input.keyboard.keys[NK_KEY_CTRL].down);
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





// Ends the input loop for the UI
void UI_InputEnd()
{
    nk_input_end(&ctx);
}





// Returns if the UI wants text input to be accepted.
// A window keeps edit.active set for as long as one of its text fields holds keyboard focus
bool UI_WantsTextInput()
{
    for (struct nk_window* win = ctx.begin; win != NULL; win = win->next)
    {
        if (win->edit.active)
            return true;
    }

    return false;
}





// Returns the Nuklear UI context
struct nk_context* UI_GetContext()
{
    return &ctx;
}





// Supplies clipboard operations from the engine integration layer
void UI_SetClipboardCallbacks(UIClipboardSetCallback set_text, UIClipboardGetCallback get_text, UIClipboardFreeCallback free_text)
{
    clipboard_set_text = set_text;
    clipboard_get_text = get_text;
    clipboard_free_text = free_text;
}





// Starts rendering a UI window with chosen flags
bool UI_BeginWindow(const char* id, const char* title, float x, float y, float width, float height, int flags)
{
    return nk_begin_titled(&ctx, id, title, nk_rect(x, y, width, height), (nk_flags)flags);
}





// Ends immediate rendering of a UI window
void UI_EndWindow()
{
    nk_end(&ctx);
}





// Lays a dynamic row in a UI window
void UI_LayoutRowDynamic(float item_height, int cols)
{
    nk_layout_row_dynamic(&ctx, item_height, cols);
}





// Lays a fixed-width row in a UI window
void UI_LayoutRowStatic(float item_height, int item_width, int cols)
{
    nk_layout_row_static(&ctx, item_height, item_width, cols);
}





// Draws a UI button with a label
bool UI_Button(const char* label)
{
    return nk_button_label(&ctx, label);
}





// Draws a basic label in a window
void UI_Label(const char* text)
{
    nk_label(&ctx, text, NK_TEXT_LEFT);
}





// Draws a label that wraps to fit its layout bounds
void UI_LabelWrapped(const char* text)
{
    nk_label_wrap(&ctx, text);
}





// Draws an item that can be selected and deselected
bool UI_Selectable(const char* label, bool* selected)
{
    int is_selected = *selected ? 1 : 0;
    bool changed = nk_selectable_label(&ctx, label, NK_TEXT_LEFT, &is_selected);
    *selected = is_selected != 0;
    return changed;
}





// Draws a slider that counts in floats
bool UI_SliderFloat(float min, float* val, float max, float step)
{
    return nk_slider_float(&ctx, min, val, max, step);
}





// Draws a slider that counts in ints
bool UI_SliderInt(int min, int* val, int max, int step)
{
    return nk_slider_int(&ctx, min, val, max, step);
}





// Draws an optionally interactive progress bar
bool UI_ProgressBar(uint32_t* value, uint32_t max, bool modifiable)
{
    nk_size current = (nk_size)*value;
    bool changed = nk_progress(&ctx, &current, (nk_size)max, modifiable ? 1 : 0);
    *value = (uint32_t)current;
    return changed;
}





// Draws a checkbox
bool UI_Checkbox(const char* label, bool* active)
{
    int is_active = *active ? 1 : 0;
    bool clicked = nk_checkbox_label(&ctx, label, &is_active);
    *active = is_active != 0;
    return clicked;
}





// Draws an active/non-active radio button 
bool UI_RadioButton(const char* label, bool active)
{
    return nk_option_label(&ctx, label, active ? 1 : 0);
}





// Draws a property window
void UI_PropertyInt(const char* name, int min, int* val, int max, int step, float inc_per_pixel)
{
    nk_property_int(&ctx, name, min, val, max, step, inc_per_pixel);
}





// Draws a float property with drag, buttons, and direct text input
void UI_PropertyFloat(const char* name, float min, float* val, float max, float step, float inc_per_pixel)
{
    nk_property_float(&ctx, name, min, val, max, step, inc_per_pixel);
}





// Draws a color picker with the output pointer
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





// Draws a combo box with chosen fields
bool UI_Combo(const char** items, int count, int* selected, int item_height, float width, float height)
{
    return nk_combobox(&ctx, items, count, selected, item_height, nk_vec2(width, height));
}





// Draws a text box for input
void UI_TextBox(char* buffer, int max_len)
{
    nk_edit_string_zero_terminated(&ctx, NK_EDIT_FIELD, buffer, max_len, nk_filter_default);
}





// Draws a multiline text input area
void UI_TextArea(char* buffer, int max_len)
{
    nk_edit_string_zero_terminated(&ctx, NK_EDIT_BOX, buffer, max_len, nk_filter_default);
}





// Starts a scrollable group panel in the next layout cell
bool UI_BeginGroup(const char* id, const char* title, int flags)
{
    return nk_group_begin_titled(&ctx, id, title, (nk_flags)flags);
}





// Ends the current group. Only call this when UI_BeginGroup returned true.
void UI_EndGroup(void)
{
    nk_group_end(&ctx);
}





// Draws a horizontal separator using the provided color
void UI_Separator(Color color, bool rounded)
{
    nk_rule_horizontal(&ctx, nk_rgba_f(color.r, color.g, color.b, color.a), rounded ? 1 : 0);
}





// Shows a tooltip when the next submitted widget is hovered over
void UI_Tooltip(const char* text)
{
    if (text && nk_widget_is_hovered(&ctx))
        nk_tooltip(&ctx, text);
}





// Sets the theme of the UI globally
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





// Sets the color style of a specific UI element
void UI_SetElementStyleColor(int element_color_id, Color color)
{
    if (element_color_id >= 0 && element_color_id < NK_COLOR_COUNT)
    {
        current_theme_table[element_color_id] = nk_rgba_f(color.r, color.g, color.b, color.a);
        nk_style_from_table(&ctx, current_theme_table);
    }
}