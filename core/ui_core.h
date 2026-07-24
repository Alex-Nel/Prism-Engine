#ifndef CORE_UI_H
#define CORE_UI_H

#include "event.h"

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



// UI Widget functions

bool UI_BeginWindow(const char* title, float x, float y, float width, float height);
void UI_EndWindow(void);
void UI_LayoutRowDynamic(float item_height, int cols);
bool UI_Button(const char* label);
void UI_Label(const char* text);
bool UI_SliderFloat(float min, float* val, float max, float step);



#endif