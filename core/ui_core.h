#ifndef CORE_UI_H
#define CORE_UI_H

#include "event_core.h"
#include "color_core.h"



struct nk_context;

typedef bool (*UIClipboardSetCallback)(const char* text);
typedef char* (*UIClipboardGetCallback)(void);
typedef void (*UIClipboardFreeCallback)(char* text);



// Initialize the UI system
void UI_Init();

// Shut down the UI system
void UI_Shutdown();

// Begin input processing for the frame
void UI_InputBegin();

// Process a single engine event
// Returns true if the UI used the event
bool UI_ProcessEvent(Event* e);

// End input processing for the frame
void UI_InputEnd();

// Returns if the UI wants text input to be accepted. Safe to call more than once per frame since it only reads the state.
bool UI_WantsTextInput();

// Get the global nuklear context
struct nk_context* UI_GetContext();

// Supplies optional clipboard operations without coupling core UI to other modules.
// get_text must return owned text that can be released by free_text.
void UI_SetClipboardCallbacks(UIClipboardSetCallback set_text, UIClipboardGetCallback get_text, UIClipboardFreeCallback free_text);



// UI Window Flags
typedef enum UIWindowFlags
{
    UI_WINDOW_BORDERED          = 1 << 0,
    UI_WINDOW_MOVABLE           = 1 << 1,
    UI_WINDOW_SCALABLE          = 1 << 2,
    UI_WINDOW_CLOSABLE          = 1 << 3,
    UI_WINDOW_MINIMIZABLE       = 1 << 4,
    UI_WINDOW_NO_SCROLLBAR      = 1 << 5,
    UI_WINDOW_TITLE             = 1 << 6,
    UI_WINDOW_SCROLL_AUTO_HIDE  = 1 << 7,
    UI_WINDOW_BACKGROUND        = 1 << 8,
    UI_WINDOW_SCALE_LEFT        = 1 << 9,
    UI_WINDOW_NO_INPUT          = 1 << 10,
    UI_WINDOW_DEFAULT           = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 4) | (1 << 6)
} UIWindowFlags;



// --- UI Widget functions ---

bool UI_BeginWindow(const char* id, const char* title, float x, float y, float width, float height, int flags);
void UI_EndWindow(void);
void UI_LayoutRowDynamic(float item_height, int cols);
void UI_LayoutRowStatic(float item_height, int item_width, int cols);
bool UI_Button(const char* label);
void UI_Label(const char* text);
void UI_LabelWrapped(const char* text);
bool UI_Selectable(const char* label, bool* selected);
bool UI_SliderFloat(float min, float* val, float max, float step);
bool UI_SliderInt(int min, int* val, int max, int step);
bool UI_ProgressBar(uint32_t* value, uint32_t max, bool modifiable);
bool UI_Checkbox(const char* label, bool* active);
bool UI_RadioButton(const char* label, bool active);
void UI_PropertyInt(const char* name, int min, int* val, int max, int step, float inc_per_pixel);
void UI_PropertyFloat(const char* name, float min, float* val, float max, float step, float inc_per_pixel);
bool UI_ColorPicker(Color* color);
bool UI_Combo(const char** items, int count, int* selected, int item_height, float width, float height);
void UI_TextBox(char* buffer, int max_len);
void UI_TextArea(char* buffer, int max_len);
bool UI_BeginGroup(const char* id, const char* title, int flags);
void UI_EndGroup(void);
void UI_Separator(Color color, bool rounded);
void UI_Tooltip(const char* text);



// --- Theming functions ---

void UI_SetTheme(int theme_id); // 0 = Dark, 1 = Light, 2 = Red, 3 = Blue
void UI_SetElementStyleColor(int element_color_id, Color color);



#endif