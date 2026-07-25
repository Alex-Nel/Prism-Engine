#ifndef CORE_UI_H
#define CORE_UI_H

#include "event.h"
#include "color.h"

struct nk_context;



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

// Get the global nuklear context
struct nk_context* UI_GetContext();



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

bool UI_BeginWindow(const char* title, float x, float y, float width, float height, int flags);
void UI_EndWindow(void);
void UI_LayoutRowDynamic(float item_height, int cols);
bool UI_Button(const char* label);
void UI_Label(const char* text);
bool UI_SliderFloat(float min, float* val, float max, float step);

bool UI_SliderInt(int min, int* val, int max, int step);
bool UI_Checkbox(const char* label, bool* active);
bool UI_RadioButton(const char* label, bool active);
void UI_PropertyInt(const char* name, int min, int* val, int max, int step, float inc_per_pixel);
bool UI_ColorPicker(Color* color);
bool UI_Combo(const char** items, int count, int* selected, int item_height, float width, float height);



// --- Theming functions ---

void UI_SetTheme(int theme_id); // 0 = Dark, 1 = Light, 2 = Red, 3 = Blue
void UI_SetElementStyleColor(int element_color_id, Color color);



#endif