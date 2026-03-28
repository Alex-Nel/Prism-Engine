#ifndef CORE_INPUT_H
#define CORE_INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "event.h" // Needs the KeyCode and MouseButton enums

// --- Initialization & Updating ---
void Input_Init(void);

// Called when an input event is pulled from the event queue
void Input_ProcessEvent(const Event* event);

// Called at the VERY END of your main game loop frame
void Input_Update(void); 

// --- Keyboard State ---
bool Input_IsKeyDown(KeyCode key);       // Is it held down right now?
bool Input_IsKeyPressed(KeyCode key);    // Was it pressed THIS frame?
bool Input_IsKeyReleased(KeyCode key);   // Was it released THIS frame?

// --- Mouse State ---
bool Input_IsMouseButtonDown(MouseButton button);
bool Input_IsMouseButtonPressed(MouseButton button);
bool Input_IsMouseButtonReleased(MouseButton button);

// Mouse Position
void Input_GetMousePosition(int32_t* x, int32_t* y);
void Input_GetMouseDelta(int32_t* dx, int32_t* dy);
float Input_GetMouseDeltaX();
float Input_GetMouseDeltaY();
float Input_GetMouseScrollDelta();

#endif