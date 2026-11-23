#include <stdlib.h>
#include <string.h>
#include "platform/platform_core.h"

#define RGFW_IMPLEMENTATION
#define RGFW_OPENGL
#define RGFW_ADVANCED_SMOOTH_RESIZE
#define RGFW_NO_DPI
#include "RGFW.h"

#define SOKOL_TIME_IMPL
#include "sokol_time.h"

#include "tinycthreads/tinycthread.h"




// Struct for a RGFW window
struct Window
{
    RGFW_window* rgfw_window;
    uint32_t id;
    uint32_t width;
    uint32_t height;
    bool is_mouse_captured;
    bool is_minimized;
    bool is_text_input_active;
    bool should_close;
    PlatformEventWatchCallback watch_callback;
    void* watch_userdata;
    struct Window* next;
};



// Wrapper tying an RGFW OpenGL context back to its owning window
typedef struct RGFWGLContextHandle
{
    RGFW_window* window;
    RGFW_glContext* ctx;
} RGFWGLContextHandle;



// Synchronization and thread wrappers backed by TinyCThread
struct PlatformMutex
{
    mtx_t mutex;
};

struct PlatformCondition
{
    cnd_t cond;
};

struct PlatformThread
{
    thrd_t handle;
};



// Module-level state
static uint32_t g_next_window_id = 1;
static uint64_t g_main_thread_id = 0;
static Window* g_window_list = NULL;
static bool g_poll_cycle_active = false;



// 1-slot buffer for synthetic text input events paired with key presses
static Event g_pending_event;
static bool g_has_pending_event = false;



// One-time initialization for sokol_time
static once_flag g_time_once = ONCE_FLAG_INIT;



static void InitTimeSystem(void)
{
    stm_setup();
}



// Per-thread stable ID assignment via TinyCThread TSS (Needed because Win32 GetCurrentThread() returns a constant pseudo-handle)
static once_flag g_tid_once = ONCE_FLAG_INIT;
static tss_t g_tid_key;
static mtx_t g_tid_mutex;
static uint64_t g_next_tid = 1;

static void InitThreadIDSystem(void)
{
    tss_create(&g_tid_key, NULL);
    mtx_init(&g_tid_mutex, mtx_plain);
}



// Forward declarations for RGFW modal callbacks
static void OnRGFWWindowResized(RGFW_window* rgfw_win, int32_t w, int32_t h);
static void OnRGFWWindowMoved(RGFW_window* rgfw_win, int32_t x, int32_t y);
static void OnRGFWWindowRefresh(RGFW_window* rgfw_win);



// Translate RGFW keys to Engine Key enums
static KeyCode TranslateKey(RGFW_key rgfw_key, RGFW_keymod mod)
{
	bool shift = (mod & RGFW_modShift) != 0;

	switch (rgfw_key)
	{
		case RGFW_a: return KEYCODE_A;
        case RGFW_b: return KEYCODE_B;
        case RGFW_c: return KEYCODE_C;
        case RGFW_d: return KEYCODE_D;
        case RGFW_e: return KEYCODE_E;
        case RGFW_f: return KEYCODE_F;
        case RGFW_g: return KEYCODE_G;
        case RGFW_h: return KEYCODE_H;
        case RGFW_i: return KEYCODE_I;
        case RGFW_j: return KEYCODE_J;
        case RGFW_k: return KEYCODE_K;
        case RGFW_l: return KEYCODE_L;
        case RGFW_m: return KEYCODE_M;
        case RGFW_n: return KEYCODE_N;
        case RGFW_o: return KEYCODE_O;
        case RGFW_p: return KEYCODE_P;
        case RGFW_q: return KEYCODE_Q;
        case RGFW_r: return KEYCODE_R;
        case RGFW_s: return KEYCODE_S;
        case RGFW_t: return KEYCODE_T;
        case RGFW_u: return KEYCODE_U;
        case RGFW_v: return KEYCODE_V;
        case RGFW_w: return KEYCODE_W;
        case RGFW_x: return KEYCODE_X;
        case RGFW_y: return KEYCODE_Y;
        case RGFW_z: return KEYCODE_Z;
        
        case RGFW_0: return shift ? KEYCODE_RIGHTPAREN   : KEYCODE_0;
        case RGFW_1: return shift ? KEYCODE_EXCLAMATION  : KEYCODE_1;
        case RGFW_2: return shift ? KEYCODE_AT           : KEYCODE_2;
        case RGFW_3: return shift ? KEYCODE_HASH         : KEYCODE_3;
        case RGFW_4: return shift ? KEYCODE_DOLLAR       : KEYCODE_4;
        case RGFW_5: return shift ? KEYCODE_PERCENT      : KEYCODE_5;
        case RGFW_6: return shift ? KEYCODE_CARET        : KEYCODE_6;
        case RGFW_7: return shift ? KEYCODE_AMPERSAND    : KEYCODE_7;
        case RGFW_8: return shift ? KEYCODE_ASTERISK     : KEYCODE_8;
        case RGFW_9: return shift ? KEYCODE_LEFTPAREN    : KEYCODE_9;
        
        case RGFW_escape: return KEYCODE_ESCAPE;
        case RGFW_F1:     return KEYCODE_F1;
        case RGFW_F2:     return KEYCODE_F2;
        case RGFW_F3:     return KEYCODE_F3;
        case RGFW_F4:     return KEYCODE_F4;
        case RGFW_F5:     return KEYCODE_F5;
        case RGFW_F6:     return KEYCODE_F6;
        case RGFW_F7:     return KEYCODE_F7;
        case RGFW_F8:     return KEYCODE_F8;
        case RGFW_F9:     return KEYCODE_F9;
        case RGFW_F10:    return KEYCODE_F10;
        case RGFW_F11:    return KEYCODE_F11;
        case RGFW_F12:    return KEYCODE_F12;
        case RGFW_delete: return KEYCODE_DELETE;

        case RGFW_backtick:     return shift ? KEYCODE_TILDE : KEYCODE_GRAVE;
        case RGFW_minus:      return shift ? KEYCODE_UNDERSCORE : KEYCODE_MINUS;
        case RGFW_equals:     return shift ? KEYCODE_PLUS       : KEYCODE_EQUALS;
        case RGFW_backSpace:  return KEYCODE_BACKSPACE;

        case RGFW_tab:          return KEYCODE_TAB;
        case RGFW_bracket:  return shift ? KEYCODE_LEFTBRACE  : KEYCODE_LEFTBRACKET;
        case RGFW_closeBracket: return shift ? KEYCODE_RIGHTBRACE : KEYCODE_RIGHTBRACKET;
        case RGFW_backSlash:    return shift ? KEYCODE_PIPE       : KEYCODE_BACKSLASH;

        case RGFW_capsLock:      return KEYCODE_CAPSLOCK;
        case RGFW_semicolon:     return shift ? KEYCODE_COLON : KEYCODE_SEMICOLON;
        case RGFW_apostrophe:    return shift ? KEYCODE_QUOTE : KEYCODE_APOSTROPHE;
        case RGFW_return:        return KEYCODE_ENTER;

        case RGFW_comma:    return shift ? KEYCODE_LESS     : KEYCODE_COMMA;
        case RGFW_period:   return shift ? KEYCODE_GREATER  : KEYCODE_PERIOD;
        case RGFW_slash:    return shift ? KEYCODE_QUESTION : KEYCODE_SLASH;

        case RGFW_space:  return KEYCODE_SPACE;

        case RGFW_shiftL: return KEYCODE_LEFTSHIFT;
        case RGFW_shiftR: return KEYCODE_RIGHTSHIFT;
        case RGFW_controlL:  return KEYCODE_LEFTCTRL;
        case RGFW_controlR:  return KEYCODE_RIGHTCTRL;
        case RGFW_altL:   return KEYCODE_LEFTALT;
        case RGFW_altR:   return KEYCODE_RIGHTALT;

        case RGFW_up:    return KEYCODE_UPARROW;
        case RGFW_right: return KEYCODE_RIGHTARROW;
        case RGFW_down:  return KEYCODE_DOWNARROW;
        case RGFW_left:  return KEYCODE_LEFTARROW;
        
        default: return KEYCODE_UNKNOWN;
	}
}



// Returns whether a physical RGFW key can produce printable text
static bool IsPrintableKey(RGFW_key key)
{
    if (key >= RGFW_space && key <= RGFW_z)
        return true;

    switch (key)
    {
        case RGFW_backtick:
        case RGFW_minus:
        case RGFW_equals:
        case RGFW_bracket:
        case RGFW_closeBracket:
        case RGFW_backSlash:
        case RGFW_semicolon:
        case RGFW_apostrophe:
        case RGFW_comma:
        case RGFW_period:
        case RGFW_slash:
        case RGFW_kp0:
        case RGFW_kp1:
        case RGFW_kp2:
        case RGFW_kp3:
        case RGFW_kp4:
        case RGFW_kp5:
        case RGFW_kp6:
        case RGFW_kp7:
        case RGFW_kp8:
        case RGFW_kp9:
        case RGFW_kpPeriod:
        case RGFW_kpSlash:
        case RGFW_kpMultiply:
        case RGFW_kpPlus:
        case RGFW_kpMinus:
        case RGFW_kpEqual:
            return true;
        default:
            return false;
    }
}



// Translate SDL Mouse Buttons to Engine Button enums
static MouseButton TranslateMouseButton(uint8_t rgfw_button)
{
    switch (rgfw_button)
    {
        case RGFW_mouseLeft:   return MOUSE_BUTTON_LEFT;
        case RGFW_mouseRight:  return MOUSE_BUTTON_RIGHT;
        case RGFW_mouseMiddle: return MOUSE_BUTTON_MIDDLE;
        default: return MOUSE_BUTTON_MAX; // Out of bounds/unsupported
    }
}










// Invokes the registered modal watch callback when running on the main thread
static void DispatchModalEvent(RGFW_window* rgfw_win, bool update_size, int32_t w, int32_t h)
{
    if (!rgfw_win)
        return;

    Window* window = (Window*)RGFW_window_getUserPtr(rgfw_win);
    if (!window)
        return;

    if (update_size)
    {
        if (w > 0 && h > 0)
        {
            window->width = (uint32_t)w;
            window->height = (uint32_t)h;
        }
    }
    else
    {
        int32_t cur_w = 0;
        int32_t cur_h = 0;
        if (RGFW_window_getSize(rgfw_win, &cur_w, &cur_h) && cur_w > 0 && cur_h > 0)
        {
            window->width = (uint32_t)cur_w;
            window->height = (uint32_t)cur_h;
        }
    }

    // Scene-facing modal work must stay on the main event thread
    if (Platform_GetCurrentThreadID() != g_main_thread_id)
        return;

    if (window->watch_callback)
        window->watch_callback(window, window->watch_userdata);
}





static void OnRGFWWindowResized(RGFW_window* rgfw_win, int32_t w, int32_t h)
{
    DispatchModalEvent(rgfw_win, true, w, h);
}



static void OnRGFWWindowMoved(RGFW_window* rgfw_win, int32_t x, int32_t y)
{
    DispatchModalEvent(rgfw_win, false, 0, 0);
}



static void OnRGFWWindowRefresh(RGFW_window* rgfw_win)
{
    DispatchModalEvent(rgfw_win, false, 0, 0);
}










// Sets an OpenGL attribute before window creation
void Platform_SetGLAttribute(GraphicsGLAttribute attr, int value)
{
    RGFW_glHints* hints = RGFW_getGlobalHints_OpenGL();
    if (!hints)
        return;

    switch (attr)
    {
        case GRAPHICS_GL_ATTR_CONTEXT_MAJOR_VERSION:
            hints->major = (int32_t)value;
            break;

        case GRAPHICS_GL_ATTR_CONTEXT_MINOR_VERSION:
            hints->minor = (int32_t)value;
            break;
        
        case GRAPHICS_GL_ATTR_CONTEXT_PROFILE_MASK:
            if (value == GRAPHICS_GL_PROFILE_CORE)
                hints->profile = RGFW_glCore;
            else if (value == GRAPHICS_GL_PROFILE_COMPATABILITY)
                hints->profile = RGFW_glCompatibility;
            else if (value == GRAPHICS_GL_PROFILE_ES)
                hints->profile = RGFW_glES;
            break;
        
        case GRAPHICS_GL_ATTR_DOUBLEBUFFER:
            hints->doubleBuffer = (int32_t)value;
            break;
        
        case GRAPHICS_GL_ATTR_DEPTH_SIZE:
            hints->depth = (int32_t)value;
            break;
        
        case GRAPHICS_GL_ATTR_STENCIL_SIZE:
            hints->stencil = (int32_t)value;
            break;
        
        default:
            break;
    }
}





// Initializes a window with a title, width, height, and graphics API
Window* Platform_Init(const char* title, uint32_t width, uint32_t height, GraphicsAPI api)
{
	call_once(&g_time_once, InitTimeSystem);

    if (g_main_thread_id == 0)
        g_main_thread_id = Platform_GetCurrentThreadID();

    if (api == GRAPHICS_API_NONE)
    {
        Log_Info("INFO: RGFW Initialized in headless mode (No Video).");
        return NULL;
    }

    Window* win = (Window*)malloc(sizeof(Window));
    if (!win)
        return NULL;

    RGFW_windowFlags window_flags = RGFW_windowCenter;

    switch (api)
    {
        case GRAPHICS_API_OPENGL:
            window_flags |= RGFW_windowOpenGL;
            break;
        case GRAPHICS_API_VULKAN:
        case GRAPHICS_API_DIRECTX:
        case GRAPHICS_API_SOFTWARE:
        default:
            break;
    }

    win->rgfw_window = RGFW_createWindow(title ? title : "Prism Engine", 0, 0, (int32_t)width, (int32_t)height, window_flags);
    if (!win->rgfw_window)
    {
        Log_Error("ERROR: RGFW_createWindow failed.\n");
        free(win);
        return NULL;
    }

    // Do not let Escape automatically mark the window as closing
    RGFW_window_setExitKey(win->rgfw_window, RGFW_keyNULL);

    int32_t actual_w = (int32_t)width;
    int32_t actual_h = (int32_t)height;
    RGFW_window_getSize(win->rgfw_window, &actual_w, &actual_h);

    win->id = g_next_window_id++;
    win->width = actual_w > 0 ? (uint32_t)actual_w : width;
    win->height = actual_h > 0 ? (uint32_t)actual_h : height;
    win->is_mouse_captured = false;
    win->is_minimized = false;
    win->is_text_input_active = false;
    win->should_close = false;
    win->watch_callback = NULL;
    win->watch_userdata = NULL;

    // Attach wrapper so modal callbacks resolve back to Window*
    RGFW_window_setUserPtr(win->rgfw_window, win);
    
    // Register in the open-window list
    win->next = g_window_list;
    g_window_list = win;

    // Install synchronous modal callbacks (invoked during native move/resize loops)
    RGFW_setWindowResizedCallback(OnRGFWWindowResized);
    RGFW_setWindowMovedCallback(OnRGFWWindowMoved);
    RGFW_setWindowRefreshCallback(OnRGFWWindowRefresh);

    return win;
}





// Sets the watch callback function
void Platform_SetEventWatchCallback(Window* window, PlatformEventWatchCallback callback, void* user_data)
{
    if (!window)
        return;

    window->watch_callback = callback;
    window->watch_userdata = user_data;
}





// Polls RGFW events across all open windows and translates them into engine events
bool Platform_PollEvents(Event* e)
{
    if (!e)
        return false;

    if (g_has_pending_event)
    {
        *e = g_pending_event;
        g_has_pending_event = false;
        return true;
    }

    if (!g_window_list)
        return false;

    if (!g_poll_cycle_active)
    {
        RGFW_setQueueEvents(RGFW_TRUE);
        RGFW_pollEvents();
        g_poll_cycle_active = true;
    }

    RGFW_event rgfw_event;

    for (Window* cur = g_window_list; cur != NULL; cur = cur->next)
    {
    	if (!cur->rgfw_window)
    		continue;

    	while (RGFW_window_checkQueuedEvent(cur->rgfw_window, &rgfw_event))
        {
            Window* win = rgfw_event.common.win ? (Window*)RGFW_window_getUserPtr(rgfw_event.common.win) : cur;

            e->type = EVENT_NONE;
	        e->window_id = win ? win->id : 0;

	        switch (rgfw_event.type)
	        {
	            case RGFW_quit:
	                e->type = EVENT_WINDOW_CLOSE;
	                break;

	            case RGFW_windowResized:
	            {
	                int32_t w = 0;
	                int32_t h = 0;
	                if (rgfw_event.common.win)
	                    RGFW_window_getSize(rgfw_event.common.win, &w, &h);
	                if (w > 0 && h > 0)
	                {
	                    e->type = EVENT_WINDOW_RESIZE;
	                    e->window_resize.width = (uint32_t)w;
	                    e->window_resize.height = (uint32_t)h;
	                }
	                break;
	            }

	            case RGFW_focusIn:
	                e->type = EVENT_WINDOW_FOCUS_GAINED;
	                break;

	            case RGFW_focusOut:
	                e->type = EVENT_WINDOW_FOCUS_LOST;
	                break;

	            case RGFW_windowMinimized:
	                e->type = EVENT_WINDOW_MINIMIZED;
	                break;

	            case RGFW_windowRestored:
	                e->type = EVENT_WINDOW_RESTORED;
	                break;

	            case RGFW_keyPressed:
	            case RGFW_keyReleased:
	            {
	                KeyCode translatedKey = TranslateKey(rgfw_event.key.value, rgfw_event.key.mod);

	                // Synthesize EVENT_TEXT_INPUT for printable characters when text input is active
	                bool emit_text = false;
	                char text_char = '\0';
	                if (rgfw_event.type == RGFW_keyPressed &&
	                    win && win->is_text_input_active &&
	                    !(rgfw_event.key.mod & (RGFW_modControl | RGFW_modAlt | RGFW_modSuper)) &&
	                    IsPrintableKey(rgfw_event.key.value) &&
	                    rgfw_event.key.sym >= 0x20 && rgfw_event.key.sym < 0x7F)
	                {
	                    emit_text = true;
	                    text_char = (char)rgfw_event.key.sym;
	                }

	                if (translatedKey != KEYCODE_UNKNOWN)
	                {
	                	if (rgfw_event.type == RGFW_keyPressed)
	                		e->type = EVENT_KEY_PRESSED;
	                	else
	                		e->type = EVENT_KEY_RELEASED;

	                    e->key.key = translatedKey;

	                    if (emit_text)
	                    {
	                        memset(&g_pending_event, 0, sizeof(g_pending_event));
	                        g_pending_event.type = EVENT_TEXT_INPUT;
	                        g_pending_event.window_id = e->window_id;
	                        g_pending_event.text_input.text[0] = text_char;
	                        g_pending_event.text_input.text[1] = '\0';
	                        g_has_pending_event = true;
	                    }
	                }
	                else if (emit_text)
	                {
	                    e->type = EVENT_TEXT_INPUT;
	                    e->text_input.text[0] = text_char;
	                    e->text_input.text[1] = '\0';
	                }
	                break;
	            }

	            case RGFW_mousePosChanged:
	                e->type = EVENT_MOUSE_MOVED;
	                e->mouse_state.x = (float)rgfw_event.mouse.x;
	                e->mouse_state.y = (float)rgfw_event.mouse.y;
	                e->mouse_state.dx = rgfw_event.mouse.vecX;
	                e->mouse_state.dy = rgfw_event.mouse.vecY;
	                break;

	            case RGFW_mouseButtonPressed:
	            case RGFW_mouseButtonReleased:
	            	if (rgfw_event.type == RGFW_mouseButtonPressed)
	            		e->type = EVENT_MOUSE_BUTTON_PRESSED;
	            	else
	            		e->type = EVENT_MOUSE_BUTTON_RELEASED;
	                e->mouse_button.button = TranslateMouseButton(rgfw_event.button.value);
	                if (e->mouse_button.button == MOUSE_BUTTON_MAX)
	                    e->type = EVENT_NONE;
	                break;

	            case RGFW_mouseScroll:
	                e->type = EVENT_MOUSEWHEEL_SCROLLED;
	                e->mouse_scroll.delta_x = rgfw_event.scroll.x;
	                e->mouse_scroll.delta_y = rgfw_event.scroll.y;
	                break;

	            default:
	                break;
	        }

	        if (e->type != EVENT_NONE)
            	return true;
        }
    }

    g_poll_cycle_active = false;
    return false;
}





// Shuts down a platform window
void Platform_Shutdown(Window* window)
{
    if (window)
    {
        // Unlink from the open-window list
        Window** link = &g_window_list;
        while (*link)
        {
            if (*link == window)
            {
                *link = window->next;
                break;
            }
            link = &(*link)->next;
        }
        window->next = NULL;

        if (g_has_pending_event && g_pending_event.window_id == window->id)
            g_has_pending_event = false;

        if (window->rgfw_window)
        {
            if (window->is_mouse_captured)
            {
                RGFW_window_unholdMouse(window->rgfw_window);
                RGFW_window_showMouse(window->rgfw_window, RGFW_TRUE);
            }

            RGFW_window_setUserPtr(window->rgfw_window, NULL);
            RGFW_window_close(window->rgfw_window);
        }

        free(window);
    }

    if (!g_window_list)
    {
        RGFW_setWindowResizedCallback(NULL);
        RGFW_setWindowMovedCallback(NULL);
        RGFW_setWindowRefreshCallback(NULL);
        g_poll_cycle_active = false;
        g_has_pending_event = false;
    }
}





// Returns the native window handle
void* Platform_GetNativeWindow(Window* window)
{
    if (!window)
        return NULL;

    return window->rgfw_window;
}





// Returns the platform-independent identifier assigned to a window
uint32_t Platform_GetWindowID(Window* window)
{
    if (!window)
        return 0;

    return window->id;
}





// Gets the x and y position of the window (from the top left)
void Platform_GetWindowPosition(Window* window, int* x, int* y)
{
    if (!window || !window->rgfw_window)
        return;

    int32_t wx = 0;
    int32_t wy = 0;

    RGFW_window_getPosition(window->rgfw_window, &wx, &wy);
    
    if (x) *x = (int)wx;
    if (y) *y = (int)wy;
}





// Returns a window's width
uint32_t Platform_GetWindowWidth(Window* window)
{
    if (!window)
        return 0;

    return window->width;
}





// Returns a window's height
uint32_t Platform_GetWindowHeight(Window* window)
{
    if (!window)
        return 0;

    return window->height;
}





// Sets cached window dimensions
void Platform_SetWindowSize(Window* window, uint32_t width, uint32_t height)
{
    if (!window)
        return;

    window->width = width;
    window->height = height;
}





// Sets the window to minimized or shown
void Platform_SetWindowMinimized(Window* window, bool minimized)
{
    if (!window || !window->rgfw_window)
        return;

    if (window->is_minimized == minimized)
        return;

    window->is_minimized = minimized;

    if (minimized)
    {
        if (!RGFW_window_isMinimized(window->rgfw_window))
            RGFW_window_minimize(window->rgfw_window);
    }
    else
    {
        if (RGFW_window_isMinimized(window->rgfw_window))
            RGFW_window_restore(window->rgfw_window);
    }
}





// Returns whether the window is currently minimized
bool Platform_IsWindowMinimized(Window* window)
{
    if (!window)
        return false;

    return window->is_minimized;
}





// Sets the window's icon from raw pixel data
bool Platform_SetWindowIcon(Window* window, const void* pixels, uint32_t width, uint32_t height, uint32_t channels)
{
    if (!window || !window->rgfw_window || !pixels || width == 0 || height == 0)
        return false;

    RGFW_format format;
    if (channels == 4)
        format = RGFW_formatRGBA8;
    else if (channels == 3)
        format = RGFW_formatRGB8;
    else
    {
        Log_Error("ERROR: Unsupported channel count for window icon: %u (expected 3 or 4)\n", channels);
        return false;
    }

    return RGFW_window_setIcon(window->rgfw_window, (uint8_t*)pixels, (int32_t)width, (int32_t)height, format) == RGFW_TRUE;
}





// High-resolution monotonic timer in seconds
double Platform_GetTime()
{
	call_once(&g_time_once, InitTimeSystem);
    return stm_sec(stm_now());
}





// Sleeps the calling thread for the specified milliseconds
void Platform_Delay(uint32_t ms)
{
	struct timespec req;
    req.tv_sec = (time_t)(ms / 1000u);
    req.tv_nsec = (long)((ms % 1000u) * 1000000u);
    while (thrd_sleep(&req, &req) == -1) {}
}





// Raises the window to the foreground
bool Platform_RaiseWindow(Window* window)
{
    if (!window || !window->rgfw_window)
        return false;

    RGFW_window_raise(window->rgfw_window);
    return true;
}





// Returns whether the mouse is captured
bool Platform_IsMouseCaptured(Window* window)
{
    if (!window)
        return false;

    return window->is_mouse_captured;
}





// Enables or disables relative mouse mode (locked + hidden + raw deltas)
void Platform_SetRelativeMouseMode(Window* window, bool enabled)
{
    if (!window || !window->rgfw_window)
        return;

    if (enabled)
    {
        RGFW_window_showMouse(window->rgfw_window, RGFW_FALSE);
        RGFW_window_holdMouse(window->rgfw_window);
    }
    else
    {
        RGFW_window_unholdMouse(window->rgfw_window);
        RGFW_window_showMouse(window->rgfw_window, RGFW_TRUE);
    }

    window->is_mouse_captured = enabled;
}





// Moves mouse cursor to the center of the window
void Platform_WarpMouseToMiddle(Window* window)
{
    if (!window || !window->rgfw_window)
        return;

    int32_t wx = 0;
    int32_t wy = 0;
    RGFW_window_getPosition(window->rgfw_window, &wx, &wy);
    RGFW_window_moveMouse(window->rgfw_window, wx + (int32_t)(window->width / 2), wy + (int32_t)(window->height / 2));
}





// Starts accepting text input from a window
void Platform_StartTextInput(Window* window)
{
    if (window)
        window->is_text_input_active = true;
}





// Stops accepting text input from a window
void Platform_StopTextInput(Window* window)
{
    if (window)
        window->is_text_input_active = false;
}





// Returns whether a window is currently accepting text input
bool Platform_IsTextInputActive(Window* window)
{
    if (!window)
        return false;

    return window->is_text_input_active;
}





// Copies UTF-8 text to the system clipboard
bool Platform_SetClipboardText(const char* text)
{
    if (!text)
        return false;

    RGFW_writeClipboard(text, (uint32_t)strlen(text));
    return true;
}





// Returns heap-allocated UTF-8 text from the system clipboard
char* Platform_GetClipboardText()
{
    size_t size = 0;
    const char* src = RGFW_readClipboard(&size);
    if (!src)
        return NULL;

    char* copy = (char*)malloc(size + 1);
    if (!copy)
        return NULL;

    if (size > 0)
        memcpy(copy, src, size);
    copy[size] = '\0';
    return copy;
}





// Releases clipboard text returned by Platform_GetClipboardText
void Platform_FreeClipboardText(char* text)
{
    free(text);
}















// ----- OpenGL specific platform functions -----

// Returns (or creates) the OpenGL context for the window
void* Platform_GL_CreateContext(void* native_window)
{
    if (!native_window)
        return NULL;

    RGFW_window* rgfw_win = (RGFW_window*)native_window;
    RGFW_glContext* ctx = RGFW_window_getContext_OpenGL(rgfw_win);

    if (!ctx)
        ctx = RGFW_window_createContext_OpenGL(rgfw_win, RGFW_getGlobalHints_OpenGL());

    if (!ctx)
        return NULL;

    RGFWGLContextHandle* handle = (RGFWGLContextHandle*)malloc(sizeof(RGFWGLContextHandle));
    if (!handle)
        return NULL;

    handle->window = rgfw_win;
    handle->ctx = ctx;
    return handle;
}





// Destroys an OpenGL context and detaches it from the window to prevent double-free
void Platform_GL_DestroyContext(void* context)
{
    if (!context)
        return;

    RGFWGLContextHandle* handle = (RGFWGLContextHandle*)context;
    if (handle->window && handle->ctx && handle->window->src.ctx.native == handle->ctx)
    {
        RGFW_window_deleteContext_OpenGL(handle->window, handle->ctx);
        handle->window->src.ctx.native = NULL;
    }

    free(handle);
}





// Makes the given OpenGL context current on the calling thread
bool Platform_GL_MakeCurrent(void* native_window, void* context)
{
    if (!native_window || !context)
    {
        RGFW_window_makeCurrentContext_OpenGL(NULL);
        return true;
    }

    RGFW_window_makeCurrentContext_OpenGL((RGFW_window*)native_window);
    return RGFW_getCurrentContext_OpenGL() != NULL;
}





// Releases the current OpenGL context from the calling thread
void Platform_GL_ReleaseCurrent(void)
{
    RGFW_window_makeCurrentContext_OpenGL(NULL);
}





// Swaps OpenGL buffers on a platform window
void Platform_GL_SwapBuffers(void* native_window)
{
    if (!native_window)
        return;

    RGFW_window_swapBuffers_OpenGL((RGFW_window*)native_window);
}





// Sets the OpenGL swap interval (VSync)
void Platform_GL_SetSwapInterval(int interval)
{
    RGFW_window* win = RGFW_getCurrentWindow_OpenGL();
    if (!win && g_window_list)
        win = g_window_list->rgfw_window;

    if (!win)
        return;

    RGFW_window_swapInterval_OpenGL(win, (int32_t)interval);
}





// Returns an OpenGL function pointer
void* Platform_GL_GetProcAddress(const char* name)
{
    return (void*)RGFW_getProcAddress_OpenGL(name);
}















// ----- Synchronization -----

// Creates a mutex from the platform
PlatformMutex* Platform_CreateMutex()
{
	PlatformMutex* mutex = (PlatformMutex*)malloc(sizeof(PlatformMutex));
    if (!mutex)
        return NULL;

    if (mtx_init(&mutex->mutex, mtx_plain) != thrd_success)
    {
        free(mutex);
        return NULL;
    }

    return mutex;
}





// Destroys a mutex
void Platform_DestroyMutex(PlatformMutex* mutex)
{
	if (!mutex)
        return;

    mtx_destroy(&mutex->mutex);
    free(mutex);
}





// Locks a mutex
void Platform_LockMutex(PlatformMutex* mutex)
{
	if (!mutex)
        return;

    mtx_lock(&mutex->mutex);
}





// Unlocks a mutex
void Platform_UnlockMutex(PlatformMutex* mutex)
{
	if (!mutex)
        return;

    mtx_unlock(&mutex->mutex);
}










// Creates a condition from the platform
PlatformCondition* Platform_CreateCondition()
{
	PlatformCondition* condition = (PlatformCondition*)malloc(sizeof(PlatformCondition));
    if (!condition)
        return NULL;

    if (cnd_init(&condition->cond) != thrd_success)
    {
        free(condition);
        return NULL;
    }

    return condition;
}





// Destroys a condition
void Platform_DestroyCondition(PlatformCondition* condition)
{
	if (!condition)
        return;

    cnd_destroy(&condition->cond);
    free(condition);
}





// Signals a condition
void Platform_SignalCondition(PlatformCondition* condition)
{
	if (!condition)
        return;

    cnd_signal(&condition->cond);
}





// Broadcasts a condition
void Platform_BroadcastCondition(PlatformCondition* condition)
{
	if (!condition)
        return;

    cnd_broadcast(&condition->cond);
}





// Waits until a condition is met
void Platform_WaitCondition(PlatformCondition* condition, PlatformMutex* mutex)
{
	if (!condition || !mutex)
        return;

    cnd_wait(&condition->cond, &mutex->mutex);
}





// Waits until a condition is met or a certain time has passed
bool Platform_WaitConditionTimeout(PlatformCondition* condition, PlatformMutex* mutex, uint32_t timeout_ms)
{
	if (!condition || !mutex)
        return false;

    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    ts.tv_sec += (time_t)(timeout_ms / 1000u);
    ts.tv_nsec += (long)((timeout_ms % 1000u) * 1000000u);
    if (ts.tv_nsec >= 1000000000L)
    {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    return cnd_timedwait(&condition->cond, &mutex->mutex, &ts) == thrd_success;
}










// Creates a joinable platform thread
PlatformThread* Platform_CreateThread(PlatformThreadFunction function, const char* name, void* user_data)
{
	(void)name;
    if (!function)
        return NULL;

    PlatformThread* thread = (PlatformThread*)malloc(sizeof(PlatformThread));
    if (!thread)
        return NULL;

    if (thrd_create(&thread->handle, (thrd_start_t)function, user_data) != thrd_success)
    {
        free(thread);
        return NULL;
    }

    return thread;
}





// Waits for a platform thread and releases its SDL thread object
void Platform_JoinThread(PlatformThread* thread, int* result)
{
	if (!thread)
        return;

    thrd_join(thread->handle, result);
    free(thread);
}





// Returns a stable identifier for the calling thread
uint64_t Platform_GetCurrentThreadID()
{
	call_once(&g_tid_once, InitThreadIDSystem);

    void* stored = tss_get(g_tid_key);
    if (!stored)
    {
        mtx_lock(&g_tid_mutex);
        uint64_t id = g_next_tid++;
        mtx_unlock(&g_tid_mutex);

        stored = (void*)(uintptr_t)id;
        tss_set(g_tid_key, stored);
    }

    return (uint64_t)(uintptr_t)stored;
}