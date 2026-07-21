#ifndef PRISMENGINE_H
#define PRISMENGINE_H

#include <stdbool.h>
#include <stdint.h>

// Include all modules
#include "core/core.h"
#include "audio/audio.h"
#include "render/render.h"
#include "scene/scene.h"
#include "platform/platform_core.h"
#include "assets/asset_manager.h"


// Struct for an "engine"
typedef struct PrismEngine
{
    Window* window;
    Renderer* renderer;
    bool is_running;
    float accumulator;
    uint32_t target_fps;
} PrismEngine;

typedef void (*EngineUpdateCallback)(void);





// --- Engine Lifecycle ---

// Initializes Platform, Core, and Render systems
bool Engine_Init(const char* window_title, uint32_t window_width, uint32_t window_height, uint32_t target_fps, GraphicsAPI api);

// Shuts down all systems
void Engine_Shutdown();

// Get the main window pointer
Window* Engine_GetMainWindow();

// Get the main renderer pointer
Renderer* Engine_GetRenderer();

// Allows API to set custom runtime logic
void Engine_SetPreUpdateCallback(EngineUpdateCallback callback);


void Engine_Run(Scene* active_scene);
bool Engine_IsRunning(); // Deprecated
void Engine_SetClearColor(float r, float g, float b, float a);






// Small struct to sort cameras before rendering
typedef struct ActiveCamera
{
    uint32_t entity_id;
    int render_order;
} ActiveCamera;



// --- Structures for frustum culling ---

typedef struct FrustumPlane
{
    Vector3 normal;
    float distance;
} FrustumPlane;

typedef struct Frustum
{
    FrustumPlane planes[6];
} Frustum;



// --- Main rendering functions ---

void Engine_GatherSceneLights(Scene* scene, RenderPacket* packet, DirectionalLightData* dir_lights, PointLightData* point_lights, SpotLightData* spot_lights);
void Engine_ExecuteShadowPass(Scene* scene, RenderPacket* packet);
uint32_t Engine_GatherAndSortCameras(Scene* scene, ActiveCamera* active_cameras);
void Engine_SubmitVisibleGeometry(Scene* scene, Frustum* cam_frustum, Vector3 cam_pos, uint32_t culling_masks);

void Engine_RenderScene(Scene* scene); // Main render function for a scene
void Engine_EndFrame();                // Swaps buffers, cycles Input state





// --- Utility Functions ---

// Captures the mouse to the window
void Engine_CaptureMouse();
// Releases the mouse to the OS
void Engine_ReleaseMouse();
// Returns if the mouse is currently captured by the engine
bool Engine_IsMouseCaptured();

// Set the target FPS of the engine
void Engine_SetTargetFPS(uint32_t fps);
// Returns the current target FPS
uint32_t Engine_GetTargetFPS();





// --- Functions for frustum culling and light cascades---

// Extracts the 6 planes from a view-projection matrix
Frustum Frustum_ExtractFromMatrix(Matrix4 vp);
// Checks if a sphere is inside the frustum
bool Frustum_ContainsAABB(Frustum* frustum, AABB local_aabb, Matrix4 world_matrix);

// Builds a texel-snapped light-space matrix that fully contains the eight frustum corners of one cascade slice
void ComputeCascadeLightMatrix(const Vector3 corners[8], Vector3 light_dir, Vector3 up, float light_distance, Matrix4* out_light_space, float* out_texel_world_size);
// Builds the eight world-space corners of a camera frustum slice.
void BuildFrustumSliceCorners(Vector3 cam_pos, Vector3 cam_fwd, Vector3 cam_right, Vector3 cam_up, float aspect, float tan_half, float split_near, float split_far, Vector3 corners[8]);





#endif