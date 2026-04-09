#ifndef CORE_INPUT_H
#define CORE_INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "event.h"

// Initialization function
void Input_Init();

// Called when an input event is pulled from the event queue
void Input_ProcessEvent(const Event* event);

// Called at the end of the main loop
void Input_Update(); 


// Keyboard State functions

bool Input_IsKeyDown(KeyCode key);
bool Input_IsKeyPressed(KeyCode key);
bool Input_IsKeyReleased(KeyCode key);


// Mouse State functions

bool Input_IsMouseButtonDown(MouseButton button);
bool Input_IsMouseButtonPressed(MouseButton button);
bool Input_IsMouseButtonReleased(MouseButton button);


// Mouse Position and Button functions

void Input_GetMousePosition(int32_t* x, int32_t* y);
void Input_GetMouseDelta(int32_t* dx, int32_t* dy);
float Input_GetMouseDeltaX();
float Input_GetMouseDeltaY();
float Input_GetMouseScrollDelta();

#endif