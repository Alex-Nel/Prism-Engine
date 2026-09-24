#ifndef RENDER_STRUCTURES_H
#define RENDER_STRUCTURES_H



#include "../core/math_core.h"
#include "../core/mesh_core.h"
#include "../core/color_core.h"
#include <stdint.h>
#include <stdbool.h>



// Used for invalid handles
#define RENDER_INVALID_HANDLE 0





// ------------------------
// --- Light Structures ---
// ------------------------



// Struct for directional light data
typedef struct DirectionalLightData
{
    Vector3 direction;
    Color color;
    float intensity;
    float ambient_strength;
    float shadow_box_size;
    uint8_t shadow_cascade_count;   // 1 = single shadow map; more = cascaded shadow maps; Clamped by settings
    float shadow_max_distance;      // max shadow range from camera (CSM only)
    float cascade_split_lambda;     // 0 = uniform splits, 1 = logarithmic, 0.5 = practical
    float cascade_blend_fraction;   // 0..1 slice fraction cross-faded at each split (CSM only)
    bool casts_shadows;
} DirectionalLightData;





// Struct for point light data
typedef struct PointLightData
{
    Vector3 position;
    Color color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    bool casts_shadows;
} PointLightData;





// Struct for spot light data
typedef struct SpotLightData
{
    Vector3 position;
    Vector3 direction;
    Color color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float inner_cut_off;
    float outer_cut_off;
    bool casts_shadows;
} SpotLightData;















// -------------------------------
// --- Render Probe Structures ---
// -------------------------------



// Struct for snapshot of local IBL volume. Input only.
typedef struct ReflectionProbeData
{
    uint32_t entity_id;
    Vector3 position;
    Vector3 box_extents;
    float blend_distance;
    int32_t priority;
    uint32_t capture_resolution;
    uint32_t revision;
} ReflectionProbeData;





// Capture status after DrawWorld. Valid until next DrawWorld call
typedef struct RenderProbeResult
{
    uint32_t entity_id;
    uint32_t revision;
    EnvironmentMapHandle environment;
    bool captured;
    bool dirty;
} RenderProbeResult;















// -------------------------------------
// --- Structures for mesh resources ---
// -------------------------------------



// Enum for the format of a vertex shader
typedef enum RenderVertexFormat
{
    RENDER_VERTEX_STATIC = 0,
    RENDER_VERTEX_SKINNED = 1
} RenderVertexFormat;





// Enum for the usage of a mesh
typedef enum RenderMeshUsage
{
    RENDER_MESH_STATIC = 0,
    RENDER_MESH_DYNAMIC = 1
} RenderMeshUsage;





// Struct for the description of a mesh
typedef struct RenderMeshDesc
{
    RenderVertexFormat vertex_format;
    RenderMeshUsage usage;
    const void* vertices;         // Vertex3D* or Vertex3DSkinned*; NULL for empty dynamic meshes
    uint32_t vertex_count;
    const uint32_t* indices;
    uint32_t index_count;
    uint32_t max_vertices;        // dynamic reserve; 0 = vertex_count
    uint32_t max_indices;
} RenderMeshDesc;





// Structure used to update a mesh
typedef struct RenderMeshUpdate
{
    const void* vertices;
    uint32_t vertex_count;
    const uint32_t* indices;
    uint32_t index_count;
} RenderMeshUpdate;










// ----------------------------------------
// --- Structures for texture resources ---
// ----------------------------------------



// Enum for a type of texture
typedef enum RenderTextureType
{
    RENDER_TEXTURE_2D = 0,
    RENDER_TEXTURE_CUBE = 1
} RenderTextureType;





// Enum for pixel formats
typedef enum RenderPixelFormat
{
    RENDER_FORMAT_R8 = 0,
    RENDER_FORMAT_RG8,
    RENDER_FORMAT_RGB8,
    RENDER_FORMAT_RGBA8,
    RENDER_FORMAT_RGB16F,
    RENDER_FORMAT_RGBA16F
} RenderPixelFormat;





// Enu for the texture filtering
typedef enum RenderTextureFilter
{
    RENDER_FILTER_DEFAULT = 0,    // backend chooses (OpenGL: 1x1 nearest, else linear mips)
    RENDER_FILTER_NEAREST,
    RENDER_FILTER_LINEAR
} RenderTextureFilter;





// Struct for the description of a texture
typedef struct RenderTextureDesc
{
    RenderTextureType type;
    RenderPixelFormat format;
    uint32_t width;
    uint32_t height;
    RenderTextureFilter min_filter;
    RenderTextureFilter mag_filter;
    const void* pixels;           // 2D texel data (uint8 or float)
    const void* cube_faces[6];    // right, left, top, bottom, front, back
} RenderTextureDesc;










// ---------------------------------------
// --- Structures for shader resources ---
// ---------------------------------------



// Enum for the format of a shader
typedef enum RenderShaderFormat
{
    RENDER_SHADER_GLSL_SOURCE = 0,
    RENDER_SHADER_SPIRV = 1
} RenderShaderFormat;





// Struct for the description of a shader
typedef struct RenderShaderDesc
{
    RenderShaderFormat format;
    const void* vertex_code;
    uint32_t vertex_size;         // 0 = NUL-terminated string
    const void* fragment_code;
    uint32_t fragment_size;
} RenderShaderDesc;










// -----------------------------------------
// --- Structures for material resources ---
// -----------------------------------------



// CPU-side material description uploaded to the backend
typedef struct RenderMaterialDesc
{
    ShaderHandle shader;          // 0 = default PBR path
    TextureHandle albedo;
    TextureHandle normal;
    TextureHandle metallic;
    TextureHandle roughness;
    TextureHandle ao;
    MaterialProperties properties;
} RenderMaterialDesc;










// ----------------------------------------
// --- Structures for Env Map resources ---
// ----------------------------------------



// Struct for the description of an environment map
typedef struct RenderEnvironmentMapDesc
{
    const float* hdr_pixels;   // If set, bake skybox + IBL
    uint32_t width;
    uint32_t height;
    TextureHandle skybox;      // If hdr_pixels is NULL, wrap this cubemap as skybox only
} RenderEnvironmentMapDesc;















// -------------------------------------------
// --- Structures for Rendering Submission ---
// -------------------------------------------



// What the backend should wipe before drawing this view
typedef enum RenderClearFlags
{
    RENDER_CLEAR_COLOR_AND_DEPTH = 0,
    RENDER_CLEAR_DEPTH_ONLY = 1,
    RENDER_CLEAR_NONE = 2
} RenderClearFlags;





// Flags for a submitted render item
enum
{
    RENDER_ITEM_TRANSPARENT     = 1u << 0,
    RENDER_ITEM_CAST_SHADOWS    = 1u << 1,
    RENDER_ITEM_RECEIVE_SHADOWS = 1u << 2,
    RENDER_ITEM_PROBE_CAPTURE   = 1u << 3
};





// One drawable submitted to the renderer for the current view
typedef struct RenderItem
{
    MeshHandle mesh;
    MaterialHandle material;
    Matrix4 transform;
    AABB local_bounds;            // mesh-space AABB
    Matrix4* bone_matrices;       // NULL if static, gets copied to the backend
    Color color;
    float depth_distance;         // transparent sort
    uint32_t flags;
} RenderItem;





// Struct for a cameras viewport, matrices, and clear
typedef struct RenderView
{
    uint32_t window_width;
    uint32_t window_height;

    Matrix4 view_matrix;
    Matrix4 projection_matrix;
    Vector3 camera_pos;
    
    RenderClearFlags clear_flags;
    Color clear_color;

    bool has_env_map;    // Whether this view draws the environment (skybox / IBL). Overlay cameras often turn this off.
} RenderView;





// Struct for a render packet to send to renderer
typedef struct RenderLighting
{
    Vector3 shadow_camera_pos;
    Vector3 camera_forward;
    Vector3 camera_right;
    Vector3 camera_up;
    float camera_near;
    float camera_far;
    float camera_fov;
    float camera_aspect;
    
    DirectionalLightData* dir_lights;
    uint32_t dir_light_count;
    
    PointLightData* point_lights; 
    uint32_t point_light_count;

    SpotLightData* spot_lights; 
    uint32_t spot_light_count;

    const ReflectionProbeData* reflection_probes;
    uint32_t reflection_probe_count;

    bool enable_ssao;
    Color global_ambient_color;
    float global_ambient_illumination;
    float gamma;
    float exposure;

    EnvironmentMapHandle env_map;
    bool has_probe_source_env_map;
    EnvironmentMapHandle probe_source_env_map;
} RenderLighting;





// View snapshot. Caller pointers only need to stay valid for the DrawWorld call. The backend copies items, bones, lights, and probes into its own storage.
typedef struct RenderWorld
{
    RenderView view;
    RenderLighting lighting;
    const RenderItem* items;
    uint32_t item_count;
} RenderWorld;















// ----------------------------------------
// --- Structures for Renderer Commands ---
// ----------------------------------------



typedef enum RenderCommandType
{
    RENDER_COMMAND_CREATE_MESH = 0,
    RENDER_COMMAND_UPDATE_MESH,
    RENDER_COMMAND_DESTROY_MESH,
    RENDER_COMMAND_CREATE_TEXTURE,
    RENDER_COMMAND_DESTROY_TEXTURE,
    RENDER_COMMAND_CREATE_SHADER,
    RENDER_COMMAND_DESTROY_SHADER,
    RENDER_COMMAND_CREATE_MATERIAL,
    RENDER_COMMAND_UPDATE_MATERIAL,
    RENDER_COMMAND_DESTROY_MATERIAL,
    RENDER_COMMAND_CREATE_ENVIRONMENT,
    RENDER_COMMAND_DESTROY_ENVIRONMENT,
    RENDER_COMMAND_RESIZE,
    RENDER_COMMAND_SET_SETTINGS,
    RENDER_COMMAND_GET_SETTINGS,
    RENDER_COMMAND_SET_VSYNC,
    RENDER_COMMAND_GET_PROBE_RESULTS
} RenderCommandType;



typedef struct RenderMeshUpdateCommand
{
    MeshHandle handle;
    const RenderMeshUpdate* update;
} RenderMeshUpdateCommand;



typedef struct RenderMaterialUpdateCommand
{
    MaterialHandle handle;
    const RenderMaterialDesc* desc;
} RenderMaterialUpdateCommand;



typedef struct RenderResizeCommand
{
    uint32_t width;
    uint32_t height;
} RenderResizeCommand;



typedef struct RenderProbeResultsCommand
{
    RenderProbeResult* out;
    uint32_t max_count;
} RenderProbeResultsCommand;





// Structure holding all renderer settings. Acts as the policy that the renderer uses
typedef struct RendererSettings
{
    bool enable_ssao;               // Enable or disable screen space ambient occlusion
    bool enable_shadows;            // Enable or disable shadow mapping
    bool enable_lighting;           // Enable or disable the deferred lighting pass
    bool enable_skybox;             // Enable or disable skybox rendering
    bool enable_transparency;       // Enable or disable forward transparent pass
    bool wireframe_mode;            // Render opaque geometry in wireframe mode

    uint32_t shadow_map_resolution; // e.g., 1024, 2048, 4096
    float gamma;                    // e.g., 2.2f (default)
    float exposure;                 // e.g., 1.0f (default)

    uint32_t max_draw_items;
    uint32_t max_shadow_cascades;
    uint32_t max_reflection_probes;
} RendererSettings;





#endif // RENDER_STRUCTURES_H