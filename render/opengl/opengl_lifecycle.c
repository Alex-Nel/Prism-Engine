#include "opengl_internal.h"





// Initializes an OpenGL renderer.
Renderer* OpenGL_Init(Render_LoadProcFn load_proc, uint32_t init_width, uint32_t init_height)
{
    Renderer* r = malloc(sizeof(Renderer));
    if (!r)
        return NULL;

    memset(r, 0, sizeof(Renderer));

    OpenGL_Backend* internal = malloc(sizeof(OpenGL_Backend));
    memset(internal, 0, sizeof(OpenGL_Backend));

    internal->state.window_width = init_width;
    internal->state.window_height = init_height;


    // Initialize data pools
    for (int i = 0; i < MAX_RESOURCES; i++)
    {
        internal->mesh_pool[i].active = false;
        internal->shader_pool[i].active = false;
        internal->texture_pool[i].active = false;
    }

    // Load OpenGL functions using the provided loader
    if (!gladLoadGLLoader((GLADloadproc)load_proc))
    {
        Log_Error("Failed to initialize OpenGL loader!");
        free(internal);
        free(r);
        return NULL;
    }

    // Set global OpenGL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE); // Disables drawing the inside of a mesh

    // Generate Guaranteed Default 1x1 Textures for Fallbacks & Safe Binding
    unsigned char white_pixel[4]  = { 255, 255, 255, 255 };
    unsigned char normal_pixel[4] = { 128, 128, 255, 255 };
    unsigned char black_pixel[4]  = { 0,   0,   0,   255 };

    glGenTextures(1, &internal->default_white_texture);
    glBindTexture(GL_TEXTURE_2D, internal->default_white_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenTextures(1, &internal->default_normal_texture);
    glBindTexture(GL_TEXTURE_2D, internal->default_normal_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, normal_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenTextures(1, &internal->default_black_texture);
    glBindTexture(GL_TEXTURE_2D, internal->default_black_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Reserve slots 1, 2, and 3 inside texture_pool exclusively for these default textures
    internal->texture_pool[1].id = internal->default_white_texture;
    internal->texture_pool[1].active = true;

    internal->texture_pool[2].id = internal->default_normal_texture;
    internal->texture_pool[2].active = true;

    internal->texture_pool[3].id = internal->default_black_texture;
    internal->texture_pool[3].active = true;


    
    // Generate Internal Skybox VAO/VBO
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &internal->skybox.vao);
    glGenBuffers(1, &internal->skybox.vbo);
    glBindVertexArray(internal->skybox.vao);
    glBindBuffer(GL_ARRAY_BUFFER, internal->skybox.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);



    // Generate Screen Quad
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
        1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
        1.0f, -1.0f,  1.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f
    };
    glGenVertexArrays(1, &internal->quad_vao);
    glGenBuffers(1, &internal->quad_vbo);
    glBindVertexArray(internal->quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, internal->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);



    // Generate depth map buffers for directional lights

    glGenFramebuffers(1, &internal->shadow.depthMapFBO);
    glGenTextures(1, &internal->shadow.depthMapTextureArray);

    glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.depthMapTextureArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, SHADOW_WIDTH, SHADOW_HEIGHT, MAX_SHADOW_CASCADES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    // GL_LINEAR on a depth texture with a compare mode set gives free 2x2 hardware
    // PCF (bilinear depth comparison) per tap, which the shader's sampler2DArrayShadow uses.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    
    // Use CLAMP_TO_BORDER so anything outside the cascade frustum is ignored
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, internal->shadow.depthMapFBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, internal->shadow.depthMapTextureArray, 0, 0);

    // Tell OpenGL we are not writing colors to this buffer, only depth
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind back to default screen



    // Generate spotlight depth map buffers
    
    glGenFramebuffers(1, &internal->shadow.spotDepthMapFBO);
    glGenTextures(1, &internal->shadow.spotDepthMapTextureArray);

    glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.spotDepthMapTextureArray);
    // Spotlights cover a much smaller area, so a size of 1024 is enough.
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, 1024, 1024, MAX_SHADOW_CASTING_SPOTLIGHTS, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, internal->shadow.spotDepthMapFBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, internal->shadow.spotDepthMapTextureArray, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);



    // --- Generate G-Buffer ---

    uint32_t win_w = init_width;
    uint32_t win_h = init_height;

    glGenFramebuffers(1, &internal->ssao.gBufferFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.gBufferFBO);

    // Position color buffer (use RGBA16F for GPU alignment)
    glGenTextures(1, &internal->ssao.gPosition);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, win_w, win_h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, internal->ssao.gPosition, 0);

    // Normal color buffer
    glGenTextures(1, &internal->ssao.gNormal);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, win_w, win_h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, internal->ssao.gNormal, 0);

    // Albedo + Specular color buffer
    glGenTextures(1, &internal->ssao.gAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, win_w, win_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, internal->ssao.gAlbedoSpec, 0);

    // Tell OpenGL we now have THREE color attachments
    uint32_t attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);

    // Create and attach depth buffer (renderbuffer)
    glGenRenderbuffers(1, &internal->ssao.gDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, internal->ssao.gDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, win_w, win_h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, internal->ssao.gDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Log_Error("G-Buffer Framebuffer not complete!");

    // --- Generate linear HDR lighting target ---
    glGenFramebuffers(1, &internal->deferred.lighting_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, internal->deferred.lighting_fbo);

    glGenTextures(1, &internal->deferred.lighting_texture);
    glBindTexture(GL_TEXTURE_2D, internal->deferred.lighting_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, win_w, win_h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, internal->deferred.lighting_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Log_Error("Deferred lighting framebuffer not complete!");

    // --- Generate SSAO FBOs ---
    glGenFramebuffers(1, &internal->ssao.ssaoFBO);  
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.ssaoFBO);
    glGenTextures(1, &internal->ssao.ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, win_w/2, win_h/2, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, internal->ssao.ssaoColorBuffer, 0);

    glGenFramebuffers(1, &internal->ssao.ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.ssaoBlurFBO);
    glGenTextures(1, &internal->ssao.ssaoColorBufferBlur);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.ssaoColorBufferBlur);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, win_w/2, win_h/2, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, internal->ssao.ssaoColorBufferBlur, 0);



    // Pre-fill SSAO buffers with white (1.0 = no occlusion) so the lit pass is safe before the first compute.
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.ssaoFBO);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    // 1x1 white texture used when SSAO is disabled in the forward pass.
    float ssao_fallback = 1.0f;
    glGenTextures(1, &internal->ssao.fallbackWhiteTexture);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.fallbackWhiteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, &ssao_fallback);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);



    // --- Generate SSAO Kernel & Noise Texture ---
    for (int i = 0; i < 64; ++i)
    {
        Vector3 sample = {
            ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f,
            ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f,
            ((float)rand() / (float)RAND_MAX) // Z is 0 to 1 to form a hemisphere
        };
        sample = Vector3Normalize(sample);
        
        // Push samples closer to the origin for better occlusion results
        float scale = (float)i / 64.0f;
        scale = 0.1f + (scale * scale) * (1.0f - 0.1f); // Lerp
        
        sample.x *= scale;
        sample.y *= scale;
        sample.z *= scale;
        internal->ssao.kernel[i] = sample;
    }

    float ssaoNoise[16 * 4];
    for (int i = 0; i < 16; i++)
    {
        ssaoNoise[i * 4 + 0] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        ssaoNoise[i * 4 + 1] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        ssaoNoise[i * 4 + 2] = 0.0f;
        ssaoNoise[i * 4 + 3] = 0.0f;
    }
    
    glGenTextures(1, &internal->ssao.noiseTexture);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGBA, GL_FLOAT, ssaoNoise);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // MUST repeat to tile over screen
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    OpenGL_BindDefaultFramebuffer();


    // Generate light spheres
    OpenGL_GenerateLightSphere(internal);


    // Generate Point Light Cubemap Array    
    glGenFramebuffers(1, &internal->shadow.pointDepthMapFBO);
    
    for (int i = 0; i < MAX_SHADOW_CASTING_POINT_LIGHTS; i++)
    {
        glGenTextures(1, &internal->shadow.pointDepthMaps[i]);
        glBindTexture(GL_TEXTURE_CUBE_MAP, internal->shadow.pointDepthMaps[i]);
        
        // Allocate the 6 faces individually
        for (unsigned int face = 0; face < 6; ++face)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT24, 
                         1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }
        
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, internal->shadow.pointDepthMapFBO);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);



    // Initialize all pipelines
    OpenGL_InitPipelines(internal);




    r->backend_internal_data = internal;

    r->api = GRAPHICS_API_OPENGL;
    r->Shutdown = OpenGL_Shutdown;
    r->SetViewport = OpenGL_SetViewport;
    r->SetClearColor = OpenGL_SetClearColor;
    r->Clear = OpenGL_Clear;
    r->ClearDepth = OpenGL_ClearDepth;

    r->CreateMesh = OpenGL_CreateMesh;
    r->DestroyMesh = OpenGL_DestroyMesh;

    r->CreateTexture = OpenGL_CreateTexture;
    r->DestroyTexture = OpenGL_DestroyTexture;

    r->CreateShader = OpenGL_CreateShader;
    r->DestroyShader = OpenGL_DestroyShader;

    r->CreateCubemap = OpenGL_CreateCubemap;
    r->CreateEnvironmentMap = OpenGL_CreateEnvironmentMap;
    r->CreateDynamicMesh = OpenGL_CreateDynamicMesh;
    r->CreateSkinnedMesh = OpenGL_CreateSkinnedMesh;
    r->UpdateDynamicMesh = OpenGL_UpdateDynamicMesh;
    r->UpdateMesh = OpenGL_UpdateMesh;

    r->BeginShadowPass = OpenGL_BeginShadowPass;
    r->EndShadowPass = OpenGL_EndShadowPass;
    
    r->BeginFrame = OpenGL_BeginFrame;
    r->Submit = OpenGL_Submit;
    r->EndFrame = OpenGL_EndFrame;

    r->SetSettings = OpenGL_SetSettings;
    r->GetSettings = OpenGL_GetSettings;

    r->UIinit = OpenGL_UIinit;
    r->UIShutdown = OpenGL_UIShutdown;
    r->UIRender = OpenGL_UIRender;
    
    return r;
}










// Shuts down an OpenGL renderer
void OpenGL_Shutdown(Renderer* r)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Clear out any pending draw commands
    internal->command_count = 0;

    // Garbage Collector Loop. We start at 1 because index 0 is the "Invalid/Null" handle.
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        // We could add a LOG_WARN here to tell the user they had a memory leak during runtime

        if (internal->mesh_pool[i].active)
            Render_DestroyMesh(r, (MeshHandle){i});
        
        if (internal->texture_pool[i].active)
            Render_DestroyTexture(r, (TextureHandle){i});
        
        if (internal->shader_pool[i].active)
            Render_DestroyShader(r, (ShaderHandle){i});
    }

    free(internal);
    free(r);
}










// Generates a simple low-poly UV sphere for light volumes
void OpenGL_GenerateLightSphere(OpenGL_Backend* internal)
{
    const int rings = 32;
    const int sectors = 32;
    const float PI = 3.14159265359f;

    float vertices[32 * 32 * 3];
    uint32_t indices[32 * 32 * 6];
    
    int v = 0;
    for (int r = 0; r < rings; ++r)
    {
        for (int s = 0; s < sectors; ++s)
        {
            float const y = sin(-PI/2.0f + PI * r / (float)(rings-1));
            float const x = cos(2*PI * s / (float)(sectors-1)) * sin(PI * r / (float)(rings-1));
            float const z = sin(2*PI * s / (float)(sectors-1)) * sin(PI * r / (float)(rings-1));

            vertices[v++] = x; vertices[v++] = y; vertices[v++] = z;
        }
    }

    int i = 0;

    for (int r = 0; r < rings - 1; ++r)
    {
        for (int s = 0; s < sectors - 1; ++s)
        {
            indices[i++] = r * sectors + s;
            indices[i++] = r * sectors + (s+1);
            indices[i++] = (r+1) * sectors + (s+1);
            indices[i++] = r * sectors + s;
            indices[i++] = (r+1) * sectors + (s+1);
            indices[i++] = (r+1) * sectors + s;
        }
    }

    internal->deferred.sphere_index_count = i;

    glGenVertexArrays(1, &internal->deferred.sphere_vao);
    glGenBuffers(1, &internal->deferred.sphere_vbo);
    glGenBuffers(1, &internal->deferred.sphere_ebo);

    glBindVertexArray(internal->deferred.sphere_vao);
    glBindBuffer(GL_ARRAY_BUFFER, internal->deferred.sphere_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal->deferred.sphere_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}










// Initializes all OpenGL pipelines with their default shaders
void OpenGL_InitPipelines(OpenGL_Backend* internal)
{
    // 1. Forward Pipeline (Main Lit Shaders)
    internal->forward.default_shader = OpenGL_CompileInternalShaderFromFile(internal, "Foward Default", "assets/shaders/default.vert", NULL, "assets/shaders/default.frag");
    internal->forward.animated_shader = OpenGL_CompileInternalShaderFromFile(internal, "Forward Animated", "assets/shaders/animated.vert", NULL, "assets/shaders/default.frag");
    
    // 2. Deferred Pipeline
    internal->deferred.deferred_shader = OpenGL_CompileInternalShaderFromFile(internal, "Deferred Main", "assets/shaders/deferred_light.vert", NULL, "assets/shaders/deferred_light.frag");
    internal->deferred.volume_shader = OpenGL_CompileInternalShaderFromFile(internal, "Deferred Volume", "assets/shaders/deferred_volume.vert", NULL, "assets/shaders/deferred_volume.frag");
    internal->deferred.spot_volume_shader = OpenGL_CompileInternalShaderFromFile(internal, "Deferred Spot Volume", "assets/shaders/deferred_volume.vert", NULL, "assets/shaders/deferred_spot_volume.frag");
    internal->deferred.post_shader = OpenGL_CompileInternalShaderFromFile(internal, "Deferred Post", "assets/shaders/deferred_light.vert", NULL, "assets/shaders/deferred_post.frag");

    // 3. Shadow Pipeline
    internal->shadow.static_shader = OpenGL_CompileInternalShaderFromFile(internal, "Shadow Static", "assets/shaders/shadow.vert", NULL, "assets/shaders/shadow.frag");
    internal->shadow.skinned_shader = OpenGL_CompileInternalShaderFromFile(internal, "Shadow Skinned", "assets/shaders/shadow_skinned.vert", NULL, "assets/shaders/shadow.frag");
    internal->shadow.point_static_shader = OpenGL_CompileInternalShaderFromFile(internal, "Shadow Point Static", "assets/shaders/shadow_point_static.vert", "assets/shaders/shadow_point.geom", "assets/shaders/shadow_point.frag");
    internal->shadow.point_skinned_shader = OpenGL_CompileInternalShaderFromFile(internal, "Shadow Point Skinned", "assets/shaders/shadow_point_skinned.vert", "assets/shaders/shadow_point.geom", "assets/shaders/shadow_point.frag");

    // 4. Skybox Pipeline
    internal->skybox.default_shader = OpenGL_CompileInternalShaderFromFile(internal, "Skybox", "assets/shaders/skybox.vert", NULL, "assets/shaders/skybox.frag");

    // 5. UI Pipeline
    internal->ui.shader = OpenGL_CompileInternalShaderFromFile(internal, "UI Shader", "assets/shaders/ui.vert", NULL, "assets/shaders/ui.frag");

    // 6. SSAO Pipeline
    internal->ssao.g_buffer_shader = OpenGL_CompileInternalShaderFromFile(internal, "G-Buffer", "assets/shaders/g_buffer.vert", NULL, "assets/shaders/g_buffer.frag");
    internal->ssao.g_buffer_skinned_shader = OpenGL_CompileInternalShaderFromFile(internal, "G-Buffer Skinned", "assets/shaders/g_buffer_skinned.vert", NULL, "assets/shaders/g_buffer.frag");
    internal->ssao.ssao_shader = OpenGL_CompileInternalShaderFromFile(internal, "SSAO Compute", "assets/shaders/ssao.vert", NULL, "assets/shaders/ssao.frag");
    internal->ssao.blur_shader = OpenGL_CompileInternalShaderFromFile(internal, "SSAO Blur", "assets/shaders/ssao.vert", NULL, "assets/shaders/ssao_blur.frag");

    // 7. IBL Precomputation Pipeline
    internal->ibl.equirectangular_to_cubemap = OpenGL_CompileInternalShaderFromFile(internal, "Equirectangular to Cubemap", "assets/shaders/cubemap.vert", NULL, "assets/shaders/equirectangular_to_cubemap.frag");
    internal->ibl.irradiance_convolution = OpenGL_CompileInternalShaderFromFile(internal, "Irradiance Convolution", "assets/shaders/cubemap.vert", NULL, "assets/shaders/irradiance_convolution.frag");
    internal->ibl.prefilter = OpenGL_CompileInternalShaderFromFile(internal, "Prefilter", "assets/shaders/cubemap.vert", NULL, "assets/shaders/prefilter.frag");
    internal->ibl.brdf = OpenGL_CompileInternalShaderFromFile(internal, "BRDF LUT", "assets/shaders/brdf.vert", NULL, "assets/shaders/brdf.frag");

    glGenFramebuffers(1, &internal->ibl.capture_fbo);
    glGenRenderbuffers(1, &internal->ibl.capture_rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, internal->ibl.capture_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, internal->ibl.capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, internal->ibl.capture_rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}










// Sets the size and position of the viewport
void OpenGL_SetViewport(Renderer* r, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Prevent redundant reallocations if the size hasn't actually changed
    if (internal->state.window_width != width || internal->state.window_height != height)
    {
        internal->state.window_width = width;
        internal->state.window_height = height;

        // Resize G-Buffer Textures
        glBindTexture(GL_TEXTURE_2D, internal->ssao.gPosition);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        glBindTexture(GL_TEXTURE_2D, internal->ssao.gNormal);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        glBindTexture(GL_TEXTURE_2D, internal->ssao.gAlbedoSpec);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        // Resize the linear HDR lighting target
        glBindTexture(GL_TEXTURE_2D, internal->deferred.lighting_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        // Resize G-Buffer Depth Renderbuffer
        glBindRenderbuffer(GL_RENDERBUFFER, internal->ssao.gDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        // Resize SSAO Textures (half resolution)
        uint32_t ssao_w = width / 2;
        uint32_t ssao_h = height / 2;
        if (ssao_w < 1) ssao_w = 1;
        if (ssao_h < 1) ssao_h = 1;

        glBindTexture(GL_TEXTURE_2D, internal->ssao.ssaoColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ssao_w, ssao_h, 0, GL_RED, GL_FLOAT, NULL);

        glBindTexture(GL_TEXTURE_2D, internal->ssao.ssaoColorBufferBlur);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ssao_w, ssao_h, 0, GL_RED, GL_FLOAT, NULL);
    }

    glViewport(x, y, width, height);
}









// Sets the color of the renderer to clear with
void OpenGL_SetClearColor(Renderer* renderer, float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
}









// Clears all buffers in the context
void OpenGL_Clear(Renderer* r)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}










// Clears the depth buffer of an OpenGL renderer
void OpenGL_ClearDepth(Renderer* r)
{
    // Wipe only the depth buffer so the color from previous cameras remains
    glClear(GL_DEPTH_BUFFER_BIT); 
}










// Sets OpenGL settings from a given settings struct
void OpenGL_SetSettings(Renderer* r, const RendererSettings* settings)
{
    if (!r || !r->backend_internal_data || !settings)
        return;
    
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Check if shadow map resolution changed and reallocate if necessary
    if (settings->shadow_map_resolution != internal->state.settings.shadow_map_resolution && settings->shadow_map_resolution > 0)
    {
        uint32_t new_res = settings->shadow_map_resolution;
        if (internal->shadow.depthMapTextureArray != 0)
        {
            glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.depthMapTextureArray);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, new_res, new_res, MAX_SHADOW_CASCADES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        }
    }


    internal->state.settings = *settings;
    internal->state.settings.enable_ssao = settings->enable_ssao;

    if (settings->gamma > 0.01f)
        internal->state.settings.gamma = settings->gamma;
    else
        internal->state.settings.gamma = 2.2f;
}










// Returns all the openGL settings in a struct
RendererSettings OpenGL_GetSettings(Renderer* r)
{
    if (!r || !r->backend_internal_data)
    {
        RendererSettings empty = {0};
        return empty;
    }
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    return internal->state.settings;
}