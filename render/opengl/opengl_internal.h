#include "../../core/log_core.h"
#include "../../core/ui_core.h"
#include "../../external/glad/glad.h"
#include "../render.h"

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

#define MAX_SHADOW_CASTING_SPOTLIGHTS 8
#define MAX_SHADOW_CASTING_POINT_LIGHTS 8

#define SHADOW_WIDTH SHADOW_MAP_RESOLUTION
#define SHADOW_HEIGHT SHADOW_MAP_RESOLUTION



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
    ReflectionProbeData* reflection_probe_results;

    Matrix4 light_space_matrices[MAX_SHADOW_CASCADES];
    Matrix4 spot_light_matrices[MAX_SHADOW_CASTING_SPOTLIGHTS];

    float shadow_texel_world_sizes[MAX_SHADOW_CASCADES];
    float cascade_splits[MAX_SHADOW_CASCADES - 1];
    Vector3 camera_forward;
    float shadow_camera_near;
    uint32_t shadow_cascade_count;
    float cascade_blend_fraction;

    Color global_ambient_color;
    float global_ambient_illumination;
    RendererSettings settings;

    bool has_env_map;
    EnvironmentMap env_map;
    bool has_probe_source_env_map;
    EnvironmentMap probe_source_env_map;
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
    EnvironmentMap environment;
} GLReflectionProbe;





// Struct for a render command. Contains mesh, shader, texture, material, and transform data
typedef struct RenderCommand
{
    MeshHandle mesh;
    ShaderHandle shader;

    TextureHandle albedo_map;
    TextureHandle normal_map;
    TextureHandle metallic_map;
    TextureHandle roughness_map;
    TextureHandle ao_map;
    
    MaterialProperties mat_props;
    Matrix4 transform;
    Matrix4* bone_matrices;
    bool is_transparent;
    float depth_distance;
    bool cast_shadows;
    bool receive_shadows;
    bool include_in_probe_capture;
} RenderCommand;





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

    RenderCommand command_queue[MAX_COMMANDS];
    uint32_t command_count;
    GLReflectionProbe reflection_probes[MAX_REFLECTION_PROBES];

    uint32_t quad_vao;
    uint32_t quad_vbo;
    
    RenderState state;

    GL_ForwardPipeline  forward;
    GL_DeferredPipeline deferred;
    GL_ShadowPipeline   shadow;
    GL_SSAOPipeline     ssao;
    GL_SkyboxPipeline   skybox;
    GL_UIPipeline       ui;
    GL_IBLPipeline      ibl;

    GLuint default_white_texture;
    GLuint default_normal_texture;
    GLuint default_black_texture;
} OpenGL_Backend;










// --- OpenGL Lifecycle Functions ---

Renderer* OpenGL_Init(Render_LoadProcFn load_proc, uint32_t init_width, uint32_t init_height);
void OpenGL_Shutdown(Renderer* r);
void OpenGL_GenerateLightSphere(OpenGL_Backend* internal);
void OpenGL_InitPipelines(OpenGL_Backend* internal);

void OpenGL_SetViewport(Renderer* r, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void OpenGL_SetClearColor(Renderer* renderer, float r, float g, float b, float a);
void OpenGL_Clear(Renderer* r);
void OpenGL_ClearDepth(Renderer* r);

void OpenGL_SetSettings(Renderer* r, const RendererSettings* settings);
RendererSettings OpenGL_GetSettings(Renderer* r);





// --- OpenGL Resource Management Functions ---

MeshHandle OpenGL_CreateMesh(Renderer* r, const Vertex3D* vertices, uint32_t vertex_count, const uint32_t* indices,  uint32_t index_count);
void OpenGL_DestroyMesh(Renderer* r, MeshHandle mesh);

TextureHandle OpenGL_CreateTexture(Renderer* r, const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels);
TextureHandle OpenGL_CreateTextureHDR(Renderer* r, const float* pixels, uint32_t width, uint32_t height, uint32_t channels);
void OpenGL_DestroyTexture(Renderer* r, TextureHandle texture);

EnvironmentMap OpenGL_CreateEnvironmentMap(Renderer* r, const float* hdr_pixels, uint32_t width, uint32_t height);

ShaderHandle OpenGL_CreateShader(Renderer* r, const char* vertex_source, const char* fragment_source);
ShaderHandle OpenGL_CompileInternalShader(OpenGL_Backend* internal, const char* name, const char* vertex_src, const char* geom_src, const char* fragment_src);
ShaderHandle OpenGL_CompileInternalShaderFromFile(OpenGL_Backend* internal, const char* name, const char* vert_path, const char* geom_path, const char* frag_path);
void OpenGL_DestroyShader(Renderer* r, ShaderHandle shader);

TextureHandle OpenGL_CreateCubemap(Renderer* r, const uint8_t* right, const uint8_t* left, const uint8_t* top, const uint8_t* bottom, const uint8_t* front, const uint8_t* back, uint32_t width, uint32_t height, uint32_t channels);
MeshHandle OpenGL_CreateSkinnedMesh(Renderer* r, const Vertex3DSkinned* vertices, uint32_t vertex_count, const uint32_t* indices,  uint32_t index_count);
MeshHandle OpenGL_CreateDynamicMesh(Renderer* r, uint32_t max_vertices, uint32_t max_indices);

void OpenGL_UpdateDynamicMesh(Renderer* r, MeshHandle handle, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count);
void OpenGL_UpdateMesh(Renderer* r, MeshHandle handle, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count);

uint8_t* OpenGL_RotatePixels90CW(const uint8_t* src, int w, int h, int c);
uint8_t* OpenGL_RotatePixels90CCW(const uint8_t* src, int w, int h, int c);





// --- OpenGL Shadow Pipeline Functions ---

void OpenGL_BindSSAOTexture(OpenGL_Backend* internal, GLuint program);
void OpenGL_CopyShadowState(RenderState* state, const RenderPacket* packet);
void OpenGL_UploadShadowUniforms(GLuint program, const RenderState* state);
void OpenGL_DrawShadowQueue(OpenGL_Backend* internal, const Matrix4* light_space_matrix);
void OpenGL_BeginShadowPass(Renderer* r, const RenderPacket* packet);
void OpenGL_EndShadowPass(Renderer* r);





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

void OpenGL_BeginFrame(Renderer* r, const RenderPacket* packet);
void OpenGL_Submit(Renderer* r, MeshHandle mesh, ShaderHandle shader, TextureHandle albedo, TextureHandle normal, TextureHandle metallic, TextureHandle roughness, TextureHandle ao, MaterialProperties mat_props, Matrix4 transform, Matrix4* bone_matrices, bool is_transparent, float depth_distance, bool cast_shadows, bool receive_shadows, bool include_in_probe_capture);
void OpenGL_EndFrame(Renderer* r);





// --- OpenGL UI Pipeline Functions ---

void OpenGL_UIinit(Renderer* r, void* nk_ctx_void);
void OpenGL_UIShutdown(Renderer* r);
void OpenGL_UIRender(Renderer* r, void* nk_ctx_void, uint32_t width, uint32_t height);
void OpenGL_DrawOverlay(Renderer* r, const OverlayDrawList* list, uint32_t width, uint32_t height);