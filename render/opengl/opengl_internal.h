#include "../../core/log_core.h"
#include "../../core/ui_core.h"
#include "../../external/glad/glad.h"
#include "../render.h"
#include "../shadow_cascades.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "../../external/nuklear.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>



#define MAX_DIR_LIGHTS 4
#define MAX_POINT_LIGHTS 512
#define MAX_SPOT_LIGHTS 512

#define MAX_RESOURCES 8192
#define MAX_COMMANDS 32768
#define MAX_REFLECTION_PROBES 16
#define MAX_SHADOW_CASCADES RENDER_MAX_SHADOW_CASCADES
#define SHADOW_MAP_RESOLUTION_DEFAULT 4096

#define MAX_SHADOW_CASTING_SPOTLIGHTS 8
#define MAX_SHADOW_CASTING_POINT_LIGHTS 8
#define MAX_SNAPSHOT_SKINNED 1024

#define SHADOW_WIDTH SHADOW_MAP_RESOLUTION_DEFAULT
#define SHADOW_HEIGHT SHADOW_MAP_RESOLUTION_DEFAULT



// Struct for a global render state
typedef struct RenderState
{
    uint32_t window_width;
    uint32_t window_height;
    Matrix4 view_matrix;
    Matrix4 projection_matrix;
    Vector3 camera_pos;

    DirectionalLightData dir_lights[MAX_DIR_LIGHTS];
    uint32_t dir_light_count;

    PointLightData point_lights[MAX_POINT_LIGHTS];
    uint32_t point_light_count;

    SpotLightData spot_lights[MAX_SPOT_LIGHTS];
    uint32_t spot_light_count;

    ReflectionProbeData reflection_probes[MAX_REFLECTION_PROBES];
    uint32_t reflection_probe_count;

    Matrix4 light_space_matrices[MAX_SHADOW_CASCADES];
    Matrix4 spot_light_matrices[MAX_SHADOW_CASTING_SPOTLIGHTS];

    float shadow_texel_world_sizes[MAX_SHADOW_CASCADES];
    float cascade_splits[MAX_SHADOW_CASCADES - 1];
    Vector3 camera_forward;
    Vector3 camera_right;
    Vector3 camera_up;
    Vector3 shadow_camera_pos;
    float shadow_camera_near;
    float shadow_camera_far;
    float shadow_camera_fov;
    float shadow_camera_aspect;
    uint32_t shadow_cascade_count;
    float cascade_blend_fraction;

    RenderClearFlags clear_flags;
    Color clear_color;

    Color global_ambient_color;
    float global_ambient_illumination;
    RendererSettings settings;

    bool has_env_map;
    EnvironmentMapHandle env_map;
    bool has_probe_source_env_map;
    EnvironmentMapHandle probe_source_env_map;
} RenderState;





// Struct for holding mesh data for OpenGL
typedef struct GLMesh
{
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    float bounding_radius;
    uint32_t index_count;
    uint32_t max_vertices;
    uint32_t max_indices;
    bool is_dynamic;
    bool is_skinned;
    bool active;
} GLMesh;



// Struct for holding shaders for OpenGL
typedef struct GLShader
{
    GLuint program;
    bool active;
} GLShader;



// Struct for holding textures for OpenGL
typedef struct GLTexture
{
    GLuint id;
    bool active;
} GLTexture;



// Struct for holding materials for OpenGL
typedef struct GLMaterial
{
    bool active;
    ShaderHandle shader;
    TextureHandle albedo;
    TextureHandle normal;
    TextureHandle metallic;
    TextureHandle roughness;
    TextureHandle ao;
    MaterialProperties properties;
} GLMaterial;



// Struct for holding environment maps for OpenGL
typedef struct GLEnvironmentMap
{
    bool active;
    bool has_ibl;
    bool owns_skybox;
    bool owns_irradiance;
    bool owns_prefilter;
    bool owns_brdf_lut;
    TextureHandle skybox;
    TextureHandle irradiance;
    TextureHandle prefilter;
    TextureHandle brdf_lut;
} GLEnvironmentMap;



// Struct for holding reflection probes for OpenGL
typedef struct GLReflectionProbe
{
    bool active;
    bool seen_this_frame;
    uint32_t entity_id;
    uint32_t captured_revision;
    Vector3 captured_position;
    uint32_t capture_resolution;
    uint32_t captured_global_skybox_id;
    EnvironmentMapHandle environment;
} GLReflectionProbe;





// --- Sub-Systems for the render pipeline---

typedef struct GL_ForwardPipeline
{
    ShaderHandle default_shader;
    ShaderHandle animated_shader;
} GL_ForwardPipeline;


typedef struct GL_DeferredPipeline
{
    ShaderHandle deferred_shader;
    ShaderHandle volume_shader;
    ShaderHandle spot_volume_shader;
    ShaderHandle probe_volume_shader;
    ShaderHandle post_shader;
    uint32_t lighting_fbo;
    uint32_t lighting_texture;
    uint32_t sphere_vao;
    uint32_t sphere_vbo;
    uint32_t sphere_ebo;
    uint32_t sphere_index_count;
} GL_DeferredPipeline;


typedef struct GL_ShadowPipeline
{
    // Directional Light Cascades
    GLuint depthMapFBO;
    GLuint depthMapTextureArray;

    // Spotlight shadows
    GLuint spotDepthMapFBO;
    GLuint spotDepthMapTextureArray;

    // Point Light shadows
    GLuint pointDepthMapFBO;
    GLuint pointDepthMaps[MAX_SHADOW_CASTING_POINT_LIGHTS];

    // Shaders
    ShaderHandle static_shader;
    ShaderHandle skinned_shader;
    ShaderHandle point_static_shader;
    ShaderHandle point_skinned_shader;
} GL_ShadowPipeline;


typedef struct GL_SSAOPipeline
{
    GLuint gBufferFBO;
    GLuint gPosition;
    GLuint gNormal;
    GLuint gDepth;
    GLuint gAlbedoSpec;

    GLuint ssaoFBO;
    GLuint ssaoBlurFBO;
    GLuint ssaoColorBuffer;
    GLuint ssaoColorBufferBlur;

    Vector3 kernel[64];
    GLuint noiseTexture;
    GLuint fallbackWhiteTexture;

    ShaderHandle g_buffer_shader;
    ShaderHandle g_buffer_skinned_shader;
    ShaderHandle ssao_shader;
    ShaderHandle blur_shader;
} GL_SSAOPipeline;


typedef struct GL_SkyboxPipeline
{
    uint32_t vao;
    uint32_t vbo;
    ShaderHandle default_shader;
} GL_SkyboxPipeline;


typedef struct GL_UIPipeline
{
    GLuint vbo, vao, ebo;
    ShaderHandle shader;

    GLint attrib_pos;
    GLint attrib_uv;
    GLint attrib_col;
    
    GLint uniform_tex;
    GLint uniform_proj;
    GLuint font_tex;
    
    struct nk_font_atlas atlas;
    struct nk_draw_null_texture tex_null;
} GL_UIPipeline;


typedef struct GL_OverlayPipeline
{
    GLuint vbo, vao, ebo;
    ShaderHandle shader;

    GLint attrib_pos;
    GLint attrib_uv;
    GLint attrib_col;

    GLint uniform_tex;
    GLint uniform_proj;
} GL_OverlayPipeline;


typedef struct GL_IBLPipeline
{
    ShaderHandle equirectangular_to_cubemap;
    ShaderHandle irradiance_convolution;
    ShaderHandle prefilter;
    ShaderHandle brdf;
    ShaderHandle probe_skybox;
    
    uint32_t capture_fbo;
    uint32_t capture_rbo;
    
    uint32_t cube_vao;
    uint32_t cube_vbo;
    
    uint32_t quad_vao;
    uint32_t quad_vbo;
} GL_IBLPipeline;





// The main backend of the OpenGL renderer
typedef struct OpenGL_Backend
{
    GLMesh mesh_pool[MAX_RESOURCES];
    GLShader shader_pool[MAX_RESOURCES];
    GLTexture texture_pool[MAX_RESOURCES];
    GLMaterial material_pool[MAX_RESOURCES];
    GLEnvironmentMap env_map_pool[MAX_RESOURCES];

    RenderItem command_queue[MAX_COMMANDS];
    uint32_t command_count;
    Matrix4 bone_snapshot[MAX_SNAPSHOT_SKINNED][MAX_BONES];
    uint32_t bone_snapshot_count;
    GLReflectionProbe reflection_probes[MAX_REFLECTION_PROBES];
    RenderProbeResult probe_results[MAX_REFLECTION_PROBES];
    uint32_t probe_result_count;

    uint32_t quad_vao;
    uint32_t quad_vbo;
    
    RenderState state;

    GL_ForwardPipeline  forward;
    GL_DeferredPipeline deferred;
    GL_ShadowPipeline   shadow;
    GL_SSAOPipeline     ssao;
    GL_SkyboxPipeline   skybox;
    GL_UIPipeline       ui;
    GL_OverlayPipeline  overlay;
    GL_IBLPipeline      ibl;

    GLuint default_white_texture;
    GLuint default_normal_texture;
    GLuint default_black_texture;
} OpenGL_Backend;










// Inline functions to get the max draw items for a opengl context
static inline uint32_t OpenGL_MaxDrawItems(const OpenGL_Backend* internal)
{
    uint32_t cap = internal->state.settings.max_draw_items;
    if (cap == 0 || cap > MAX_COMMANDS)
        return MAX_COMMANDS;
    return cap;
}



static inline GLEnvironmentMap* OpenGL_GetEnvMap(OpenGL_Backend* internal, EnvironmentMapHandle handle)
{
    if (!internal || handle.id == 0 || handle.id >= MAX_RESOURCES)
        return NULL;
    GLEnvironmentMap* env = &internal->env_map_pool[handle.id];
    return env->active ? env : NULL;
}



static inline GLuint OpenGL_TextureGL(OpenGL_Backend* internal, TextureHandle handle)
{
    if (!internal || handle.id == 0 || handle.id >= MAX_RESOURCES)
        return 0;
    if (!internal->texture_pool[handle.id].active)
        return 0;
    return internal->texture_pool[handle.id].id;
}





// --- OpenGL Lifecycle Functions ---

Renderer* OpenGL_Init(Render_LoadProcFn load_proc, uint32_t init_width, uint32_t init_height);
void OpenGL_Shutdown(Renderer* r);
void OpenGL_GenerateLightSphere(OpenGL_Backend* internal);
void OpenGL_InitPipelines(OpenGL_Backend* internal);

void OpenGL_SetSettings(Renderer* r, const RendererSettings* settings);
RendererSettings OpenGL_GetSettings(Renderer* r);
void OpenGL_Resize(Renderer* r, uint32_t width, uint32_t height);





// --- OpenGL Resource Management Functions ---

MeshHandle OpenGL_CreateMesh(Renderer* r, const RenderMeshDesc* desc);
void OpenGL_UpdateMesh(Renderer* r, MeshHandle handle, const RenderMeshUpdate* update);
void OpenGL_DestroyMesh(Renderer* r, MeshHandle mesh);

TextureHandle OpenGL_CreateTexture(Renderer* r, const RenderTextureDesc* desc);
void OpenGL_DestroyTexture(Renderer* r, TextureHandle texture);

EnvironmentMapHandle OpenGL_CreateEnvironmentMap(Renderer* r, const RenderEnvironmentMapDesc* desc);
void OpenGL_DestroyEnvironmentMap(Renderer* r, EnvironmentMapHandle handle);
void OpenGL_DestroyEnvMapInternal(OpenGL_Backend* internal, EnvironmentMapHandle handle);

ShaderHandle OpenGL_CreateShader(Renderer* r, const RenderShaderDesc* desc);
ShaderHandle OpenGL_CompileInternalShader(OpenGL_Backend* internal, const char* name, const char* vertex_src, const char* geom_src, const char* fragment_src);
ShaderHandle OpenGL_CompileInternalShaderFromFile(OpenGL_Backend* internal, const char* name, const char* vert_path, const char* geom_path, const char* frag_path);
void OpenGL_DestroyShader(Renderer* r, ShaderHandle shader);

MaterialHandle OpenGL_CreateMaterial(Renderer* r, const RenderMaterialDesc* desc);
void OpenGL_UpdateMaterial(Renderer* r, MaterialHandle handle, const RenderMaterialDesc* desc);
void OpenGL_DestroyMaterial(Renderer* r, MaterialHandle handle);

uint8_t* OpenGL_RotatePixels90CW(const uint8_t* src, int w, int h, int c);
uint8_t* OpenGL_RotatePixels90CCW(const uint8_t* src, int w, int h, int c);





// --- OpenGL Shadow Pipeline Functions ---

void OpenGL_BindSSAOTexture(OpenGL_Backend* internal, GLuint program);
void OpenGL_UploadShadowUniforms(GLuint program, const RenderState* state);
void OpenGL_DrawShadowQueue(OpenGL_Backend* internal, const Matrix4* light_space_matrix);
void OpenGL_ExecuteShadowPass(OpenGL_Backend* internal);





// --- OpenGL Render Pipeline Functions ---

void OpenGL_BindDefaultFramebuffer();
void OpenGL_UploadCommonUniforms(GLuint program, const RenderState* state);
void OpenGL_UploadLightUniforms(GLuint program, const RenderState* state);
void OpenGL_UploadDirectionalLightUniforms(GLuint program, const RenderState* state);
void ExecuteGBufferPass(OpenGL_Backend* internal, uint32_t opaque_count);
void ExecuteDeferredLightingPass(OpenGL_Backend* internal);
void ExecuteSSAOPass(OpenGL_Backend* internal);
void OpenGL_RenderCommandBatch(OpenGL_Backend* internal, uint32_t start_idx, uint32_t end_idx);
void OpenGL_DrawSkybox(OpenGL_Backend* internal);

void OpenGL_BeginFrame(Renderer* r, const RenderView* view, const RenderLighting* lighting);
void OpenGL_EndFrame(Renderer* r);
void OpenGL_DrawWorld(Renderer* r, const RenderWorld* world);
uint32_t OpenGL_GetProbeResults(Renderer* r, RenderProbeResult* out, uint32_t max_count);





// --- OpenGL UI Pipeline Functions ---

void OpenGL_UIinit(Renderer* r, void* nk_ctx_void);
void OpenGL_UIShutdown(Renderer* r);
void OpenGL_UIRender(Renderer* r, void* nk_ctx_void, uint32_t width, uint32_t height);
void OpenGL_OverlayInit(Renderer* r);
void OpenGL_OverlayShutdown(Renderer* r);
void OpenGL_DrawOverlay(Renderer* r, const OverlayDrawList* list, uint32_t width, uint32_t height);