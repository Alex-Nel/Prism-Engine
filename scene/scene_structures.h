#ifndef SCENE_STRUCTURES_H
#define SCENE_STRUCTURES_H

#include <stdint.h>
#include "physics_bridge.h"
#include "../audio/audio.h"
#include "../core/font_core.h"
#include "../core/mesh_core.h"
#include "../core/overlay_core.h"





#define MAX_ENTITIES 32768
#define MAX_NAME_LENGTH 256
#define MAX_SCRIPTS_PER_ENTITY 64
#define MAX_COLLISION_OVERLAPS 16
#define MAX_MATERIAL_SLOTS 256
#define MAX_LINE_POINTS 1024
#define SHADOW_CASCADE_COUNT_DEFAULT 1





// The collision layers and mask enum
typedef enum CollisionLayer
{
    COLLISION_LAYER_NONE      = 0,
    COLLISION_LAYER_DEFAULT   = (1 << 0),  // Standard colliders
    COLLISION_LAYER_TRIGGER   = (1 << 1),  // Invisible triggers

    // User defined layers
    COLLISION_LAYER_USER_1    = (1 << 2),
    COLLISION_LAYER_USER_2    = (1 << 3),
    COLLISION_LAYER_USER_3    = (1 << 4),
    COLLISION_LAYER_USER_4    = (1 << 5),
    COLLISION_LAYER_USER_5    = (1 << 6),
    COLLISION_LAYER_USER_6    = (1 << 7),
    COLLISION_LAYER_USER_7    = (1 << 8),
    COLLISION_LAYER_USER_8    = (1 << 9),
    COLLISION_LAYER_USER_9    = (1 << 10),
    COLLISION_LAYER_USER_10   = (1 << 11),
    COLLISION_LAYER_USER_11   = (1 << 12),
    COLLISION_LAYER_USER_12   = (1 << 13),
    COLLISION_LAYER_USER_13   = (1 << 14),

    COLLISION_MASK_NONE       = 0,
    COLLISION_MASK_ALL        = -1
} CollisionLayer;





// Enum for all Component Masks
typedef enum
{
    COMPONENT_NONE                     = 0,
    COMPONENT_NAME                     = 1 << 0,
    COMPONENT_TAG                      = 1 << 1,
    COMPONENT_TRANSFORM                = 1 << 2,
    COMPONENT_MESH_RENDERER            = 1 << 3,
    COMPONENT_SKINNED_MESH_RENDERER    = 1 << 4,
    COMPONENT_CAMERA                   = 1 << 5,
    COMPONENT_LIGHT                    = 1 << 6,
    COMPONENT_COLLIDER                 = 1 << 7,
    COMPONENT_RIGIDBODY                = 1 << 8,
    COMPONENT_AUDIO_LISTENER           = 1 << 9,
    COMPONENT_AUDIO_SOURCE             = 1 << 10,
    COMPONENT_ANIMATOR                 = 1 << 11,
    COMPONENT_BONE_ATTACHMENT          = 1 << 12,
    COMPONENT_LINE_RENDERER            = 1 << 13,
    COMPONENT_SPRITE_RENDERER          = 1 << 14,
    COMPONENT_REFLECTION_PROBE         = 1 << 15,
    COMPONENT_UI_CANVAS                = 1 << 16,
    COMPONENT_UI_RECT_TRANSFORM        = 1 << 17,
    COMPONENT_UI_IMAGE                 = 1 << 18,
    COMPONENT_UI_TEXT                  = 1 << 19,
    COMPONENT_UI_BUTTON                = 1 << 20,
    COMPONENT_SCRIPT                   = 1 << 21
} ComponentMask;





typedef struct Scene Scene;

// Struct for an entity
// Contains it's ID and pointer to the scene it's in
typedef struct Entity
{
    uint32_t id;
    Scene* scene;
} Entity;

// ID for an invalid entity
#define ENTITY_NONE (uint32_t) 0xFFFFFFFF





// --- Function pointers for custom scripts ---

// On Update function
typedef void (*ScriptUpdateFunc)(Entity entity, void* instance_data);
// On Update function
typedef void (*ScriptFixedUpdateFunc)(Entity entity, void* instance_data);
// On Start function
typedef void (*ScriptStartFunc)(Entity entity, void* instance_data);
// On Destroy function
typedef void (*ScriptDestroyFunc)(Entity entity, void* instance_data);
// On Enable function
typedef void (*ScriptEnableFunc)(Entity entity, void* instance_data);
// On Disable function
typedef void (*ScriptDisableFunc)(Entity entity, void* instance_data);
// On Collision function (any kind)
typedef void (*CollisionCallback)(Entity self, Entity other, void* instance_data);
// Pointer / UI event function
typedef void (*PointerCallback)(Entity entity, void* instance_data);





// -------------------------------- //
// --- All Component Structures --- //
// -------------------------------- //


// An entities name
typedef struct NameComponent
{
    char name[MAX_NAME_LENGTH];
} NameComponent;



// An entities tag
typedef struct TagComponent
{
    char tag[MAX_NAME_LENGTH];
} TagComponent;



// Transform: an entities positional parts within the scene
typedef struct Transform
{
    Entity entity;
    Vector3 local_position;
    Vector3 local_rotation_euler;
    Quaternion local_rotation;
    Vector3 local_scale;

    Matrix4 world_matrix; 

    uint32_t parent_id;
    uint32_t first_child_id;
    uint32_t next_sibling_id;
    uint32_t prev_sibling_id;
    
    bool is_dirty; 
} Transform;



// Component that holds rendering information
typedef struct MeshRendererComponent
{
    Entity entity;
    bool is_active;

    Mesh* mesh;
    Material* material;

    uint32_t layer_mask; // Only one layer mask
    bool casts_shadows;
    bool receives_shadows;
} MeshRendererComponent;



// Component that holds skinned mesh rendering information
typedef struct SkinnedMeshRendererComponent
{
    Entity entity;
    bool is_active;

    SkinnedMesh* mesh;
    Material* material;

    uint32_t layer_mask; // Only one layer mask
    bool casts_shadows;
    bool receives_shadows;

    AABB pose_bounds; // Updated each frame for frustum culling

    uint32_t root_animator_entity_id;
} SkinnedMeshRendererComponent;



// Defines what the camera wipes before drawing
typedef enum CameraClearFlags {
    CLEAR_COLOR_AND_DEPTH = 0,
    CLEAR_DEPTH_ONLY = 1,
    CLEAR_NONE = 2
} CameraClearFlags;



// Camera component for rendering
typedef struct CameraComponent
{
    Entity entity;
    bool is_active;
    float fov;
    float nearZ;
    float farZ;
    Matrix4 projection_matrix; // Projection matrix is cached to prevent recalculations
    bool is_dirty;

    uint32_t culling_masks; // Can be several layer masks

    int render_order; // Lower numbers render first
    CameraClearFlags clear_flags;

    uint32_t viewport_x;
    uint32_t viewport_y;
    uint32_t viewport_width;
    uint32_t viewport_height;
} CameraComponent;



// Enum for differnet light types
typedef enum LightType 
{
    LIGHT_DIRECTIONAL = 0,
    LIGHT_POINT = 1,
    LIGHT_SPOT = 2
} LightType;



// Point light component
typedef struct LightComponent
{
    Entity entity;
    bool is_active;
    LightType type;
    Color color;
    float intensity;

    // Ambient light
    float ambient_strength;
    
    // Attenuation (Falloff) variables (for point and spot lights):
    float constant;     // usually 1.0f
    float linear;       // smaller means light travels further
    float quadratic;    // smaller means light travels further

    // Spot light constraints (Represented in degrees)
    float inner_cut_off;
    float outer_cut_off;

    float shadow_box_size;
    uint8_t shadow_cascade_count; // 1 = single map (default) | 2-4 = CSM slices up to shadow_max_distance
    float shadow_max_distance;
    float cascade_split_lambda;   // 0 = uniform, 1 = logarithmic, 0.5 = practical
    float cascade_blend_fraction; // 0..1 slice fraction cross-faded at each split (CSM only)
    
    bool casts_shadows;
} LightComponent;



// Collider Types
typedef enum ColliderType
{
    COLLIDER_BOX,
    COLLIDER_SPHERE,
    COLLIDER_MESH
} ColliderType;



// The Collider Unified Component
typedef struct ColliderComponent
{
    Entity owner;
    bool is_active;
    ColliderType type;
    bool is_trigger;
    bool is_convex;
    
    void* physics_handle;

    Mesh* mesh_ptr;
    Vector3 extents;
    float radius;
    Vector3 mesh_scale;

    uint32_t touching_entities[MAX_COLLISION_OVERLAPS];
    uint32_t touching_count;

    CollisionLayer collision_layer;
    int collision_mask;
} ColliderComponent;



// Rigidbody component and variables
typedef struct RigidbodyComponent
{
    Entity owner;
    bool is_active;
    float mass;
    float linear_drag;
    float angular_drag;
    bool use_gravity;
    bool is_kinematic;

    bool freeze_rot_x;
    bool freeze_rot_y;
    bool freeze_rot_z;
} RigidbodyComponent;



// An audio listener component (plays audio)
typedef struct AudioListenerComponent
{
    Entity entity;
    bool is_active;
} AudioListenerComponent;



// The speaker component
typedef struct AudioSourceComponent
{
    Entity entity;
    bool is_active;
    AudioClipHandle clip;
    float volume;
    float pitch;
    
    bool loop;
    bool play_on_awake;
    bool is_playing;
    
    bool is_spatial; // True = 3D audio, False = 2D background music
    
    // 3D settings
    float min_distance; // Distance where volume starts dropping
    float max_distance; // Distance where volume becomes silent
} AudioSourceComponent;



// An animation component that plays an animation
typedef struct AnimatorComponent
{
    Entity entity;
    bool is_active;

    Skeleton* skeleton;
    AnimationClip* current_clip;

    float current_time_ticks;
    bool is_playing;
    float playback_speed;

    Matrix4* final_bone_matrices;
} AnimatorComponent;



// A bone attachment for complex models
typedef struct BoneAttachmentComponent
{
    Entity owner;
    bool is_active;
    int target_bone_index;       // The integer ID of the bone in the skeleton
    Matrix4 local_offset;  // An offset matrix to adjust how the item sits in the hand
} BoneAttachmentComponent;



// A line renderer component for 2D lines
typedef struct LineRendererComponent
{
    Entity entity;
    bool is_active;
    Vector3 points[MAX_LINE_POINTS];
    uint32_t point_count;

    float start_thickness;
    float end_thickness;
    Color color;
    bool is_loop;
    bool use_world_space;

    Mesh* dynamic_mesh;
    Material* material;
} LineRendererComponent;



// A Sprite Renderer Component
typedef struct SpriteRendererComponent
{
    Entity entity;
    bool is_active;
    Color color;

    Mesh* quad;
    Material* material;
} SpriteRendererComponent;



// A local IBL volume. Its radiance cubemap is captured once and regenerated whenever the probe revision changes.
typedef struct ReflectionProbeComponent
{
    Entity entity;
    bool is_active;

    Vector3 box_extents;
    float blend_distance;
    
    int32_t priority;
    uint32_t capture_resolution;
    uint32_t revision;
    
    bool dirty;
    bool captured;
    
    Vector3 last_capture_position;
    EnvironmentMapHandle environment;
} ReflectionProbeComponent;





// --- Retained UI Components ---

// Enum for the scale mode of a canvas
typedef enum UICanvasScaleMode
{
    UI_CANVAS_CONSTANT_PIXEL_SIZE = 0,
    UI_CANVAS_SCALE_WITH_SCREEN_SIZE = 1
} UICanvasScaleMode;



// Enum for the alignment of a UI Text component
typedef enum UITextAlignment
{
    UI_TEXT_ALIGN_LEFT,
    UI_TEXT_ALIGN_CENTER,
    UI_TEXT_ALIGN_RIGHT
} UITextAlignment;



// Enum for the state of a UI Button component
typedef enum UIButtonState
{
    UI_BUTTON_STATE_NORMAL,
    UI_BUTTON_STATE_HOVERED,
    UI_BUTTON_STATE_PRESSED,
    UI_BUTTON_STATE_DISABLED
} UIButtonState;



// Enum for each of the pointer events
typedef enum UIPointerEvent
{
    UI_POINTER_ENTER,
    UI_POINTER_EXIT,
    UI_POINTER_DOWN,
    UI_POINTER_UP,
    UI_POINTER_CLICK
} UIPointerEvent;



// A UI Canvas that contains several other UI components
typedef struct UICanvasComponent
{
    Entity entity;
    bool is_active;

    int sort_order;
    UICanvasScaleMode scale_mode;
    Vector2 reference_resolution;
    float match_width_or_height;
    bool blocks_raycasts;

    float scale_factor;
} UICanvasComponent;



// A Rect Transform, required for any UI component
typedef struct RectTransformComponent
{
    Entity entity;

    // Vector2's are interpreted as:   (0, 0) top left   (1, 1) bottom right
    Vector2 anchor_min;          // The minimum (normalized) point where the element binds itself to the parent.
    Vector2 anchor_max;          // The maximum (normalized) point where the element binds itself to the parent.
    Vector2 pivot;               // The "Center" of the UI element, where position/rotataing/scaling is relative to.
    Vector2 size_delta;          // The absolute width/height if anchors are equal, or the margins/padding if their not
    Vector2 anchored_position;   // The 'offset' if anchors are equal, or an offset from the center of the stretched rectangle.

    float screen_x;
    float screen_y;
    float screen_width;
    float screen_height;

    bool is_dirty;
} RectTransformComponent;



// A UI Image component, renders a image as an overlay
typedef struct UIImageComponent
{
    Entity entity;
    bool is_active;
    Texture* texture;
    Color color;
    bool raycast_target;
} UIImageComponent;



// A UI Text component, renders text
typedef struct UITextComponent
{
    Entity entity;
    bool is_active;
    char text[256];
    Font* font;
    Color color;
    UITextAlignment alignment;
    float font_size;
    bool wrap;
    bool raycast_target;
} UITextComponent;



// A UI Button component, renders an interactable button
typedef struct UIButtonComponent
{
    Entity entity;
    bool is_active;
    bool interactable;

    UIButtonState current_state;
    Color color_normal;
    Color color_hovered;
    Color color_pressed;
    Color color_disabled;

    bool clicked_this_frame;
} UIButtonComponent;





// Forward decleration of cJSON struct
struct cJSON;

// Instance of a custom script and all special functions
typedef struct ScriptInstance
{
    Entity entity;
    bool is_active;
    bool is_enabled_internal;
    bool has_started;
    void* instance_data;

    ScriptStartFunc OnStart;
    ScriptUpdateFunc OnUpdate;
    ScriptFixedUpdateFunc OnFixedUpdate;
    ScriptDestroyFunc OnDestroy;
    ScriptEnableFunc OnEnable;
    ScriptDisableFunc OnDisable;

    CollisionCallback OnTriggerEnter;
    CollisionCallback OnTriggerStay;
    CollisionCallback OnTriggerExit;
    
    CollisionCallback OnCollisionEnter;
    CollisionCallback OnCollisionStay;
    CollisionCallback OnCollisionExit;

    PointerCallback OnPointerEnter;
    PointerCallback OnPointerExit;
    PointerCallback OnPointerDown;
    PointerCallback OnPointerUp;
    PointerCallback OnPointerClick;

    void (*OnSerialize)(Entity entity, void* instance_data, struct cJSON* json);
    void (*OnDeserialize)(Entity entity, void* instance_data, struct cJSON* json);
} ScriptInstance;



// Custom scripting component
typedef struct ScriptComponent
{
    ScriptInstance instances[MAX_SCRIPTS_PER_ENTITY];
    uint32_t count;
} ScriptComponent;










// --- Retained UI State ---

// An entry for the order of Canvases
typedef struct UICanvasSortEntry
{
    uint32_t entity_id;
    int sort_order;
} UICanvasSortEntry;



// The state of the retained UI
typedef struct RetainedUIState
{
    OverlayDrawList draw_list;
    UICanvasSortEntry canvas_entries[MAX_ENTITIES];
    uint32_t canvas_count;

    uint32_t hovered_entity_id;
    uint32_t pressed_entity_id;
    uint32_t window_width;
    uint32_t window_height;
    bool blocks_pointer;
    bool layout_dirty;
} RetainedUIState;



// Extern variable for the state of the UI
extern RetainedUIState g_ui_state;










// --- The Scene Struct ---
typedef struct Scene
{
    uint32_t component_masks[MAX_ENTITIES];
    bool is_active_self[MAX_ENTITIES];
    bool is_active_in_hierarchy[MAX_ENTITIES];
    


    // The scenes component arrays
    
    NameComponent names[MAX_ENTITIES];
    TagComponent tags[MAX_ENTITIES];
    Transform transforms[MAX_ENTITIES];
    MeshRendererComponent mesh_renderers[MAX_ENTITIES];
    SkinnedMeshRendererComponent skinned_mesh_renderers[MAX_ENTITIES];
    CameraComponent cameras[MAX_ENTITIES];
    LightComponent lights[MAX_ENTITIES];
    ColliderComponent colliders[MAX_ENTITIES];
    RigidbodyComponent rigidbodies[MAX_ENTITIES];
    AudioListenerComponent audio_listeners[MAX_ENTITIES];
    AudioSourceComponent audio_sources[MAX_ENTITIES];
    AnimatorComponent animators[MAX_ENTITIES];
    BoneAttachmentComponent bone_attachments[MAX_ENTITIES];
    LineRendererComponent line_renderers[MAX_ENTITIES];
    SpriteRendererComponent sprite_renderers[MAX_ENTITIES];
    ReflectionProbeComponent reflection_probes[MAX_ENTITIES];
    
    UICanvasComponent ui_canvases[MAX_ENTITIES];
    RectTransformComponent ui_rect_transforms[MAX_ENTITIES];
    UIImageComponent ui_images[MAX_ENTITIES];
    UITextComponent ui_texts[MAX_ENTITIES];
    UIButtonComponent ui_buttons[MAX_ENTITIES];
    
    ScriptComponent scripts[MAX_ENTITIES];



    // Other Variables for the state of this scene

    uint32_t main_camera_id;
    PhysicsWorldHandle physics_world;
    Vector3 gravity;



    // Variables for the skybox

    bool has_env_map;
    EnvironmentMap* env_map;
    Color background_color;
    Color ambient_color;
    float ambient_illumination;
    float exposure;



    // Variables for entities to remove

    bool is_pending_destroy[MAX_ENTITIES];
    uint32_t entities_to_destroy[MAX_ENTITIES];
    uint32_t destroy_count;
} Scene;





#endif