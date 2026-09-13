#include "opengl_internal.h"



struct nk_draw_vertex
{
    float position[2];
    float uv[2];
    nk_byte col[4];
};





// Initializes the UI render context
void OpenGL_UIinit(Renderer* r, void* nk_ctx_void)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    struct nk_context* ctx = (struct nk_context*)nk_ctx_void;
    
    GLuint prog = internal->shader_pool[internal->ui.shader.id].program;
    
    internal->ui.uniform_tex = glGetUniformLocation(prog, "Texture");
    internal->ui.uniform_proj = glGetUniformLocation(prog, "ProjMtx");
    internal->ui.attrib_pos = glGetAttribLocation(prog, "Position");
    internal->ui.attrib_uv = glGetAttribLocation(prog, "TexCoord");
    internal->ui.attrib_col = glGetAttribLocation(prog, "Color");
    
    glGenBuffers(1, &internal->ui.vbo);
    glGenBuffers(1, &internal->ui.ebo);
    glGenVertexArrays(1, &internal->ui.vao);
    
    glBindVertexArray(internal->ui.vao);
    glBindBuffer(GL_ARRAY_BUFFER, internal->ui.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal->ui.ebo);
    
    glEnableVertexAttribArray((GLuint)internal->ui.attrib_pos);
    glEnableVertexAttribArray((GLuint)internal->ui.attrib_uv);
    glEnableVertexAttribArray((GLuint)internal->ui.attrib_col);
    
    glVertexAttribPointer((GLuint)internal->ui.attrib_pos, 2, GL_FLOAT, GL_FALSE, sizeof(struct nk_draw_vertex), (void*)offsetof(struct nk_draw_vertex, position));
    glVertexAttribPointer((GLuint)internal->ui.attrib_uv, 2, GL_FLOAT, GL_FALSE, sizeof(struct nk_draw_vertex), (void*)offsetof(struct nk_draw_vertex, uv));
    glVertexAttribPointer((GLuint)internal->ui.attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(struct nk_draw_vertex), (void*)offsetof(struct nk_draw_vertex, col));
    
    glBindVertexArray(0);
    
    nk_font_atlas_init_default(&internal->ui.atlas);
    nk_font_atlas_begin(&internal->ui.atlas);
    
    struct nk_font *font = nk_font_atlas_add_default(&internal->ui.atlas, 13.0f, 0);
    const void *image;
    int w, h;
    image = nk_font_atlas_bake(&internal->ui.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    
    RenderTextureDesc font_desc = {0};
    font_desc.type = RENDER_TEXTURE_2D;
    font_desc.format = RENDER_FORMAT_RGBA8;
    font_desc.width = (uint32_t)w;
    font_desc.height = (uint32_t)h;
    font_desc.min_filter = RENDER_FILTER_LINEAR;
    font_desc.mag_filter = RENDER_FILTER_LINEAR;
    font_desc.pixels = image;
    internal->ui.font_texture = r->CreateTexture(r, &font_desc);

    nk_font_atlas_end(&internal->ui.atlas, nk_handle_id((int)internal->ui.font_texture.id), &internal->ui.tex_null);
    
    // Slot 1 is the renderer's guaranteed white texture.
    internal->ui.tex_null.texture = nk_handle_id(1);
    internal->ui.tex_null.uv = nk_vec2(0.5f, 0.5f);
    UI_SetRenderTextureHandles((TextureHandle){1}, 0.5f, 0.5f);

    if (font)
        nk_style_set_font(ctx, &font->handle);
}










// Shuts down the UI rendering context
void OpenGL_UIShutdown(Renderer* r)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    r->DestroyShader(r, internal->ui.shader);
    glDeleteBuffers(1, &internal->ui.vbo);
    glDeleteBuffers(1, &internal->ui.ebo);
    glDeleteVertexArrays(1, &internal->ui.vao);
    r->DestroyTexture(r, internal->ui.font_texture);
    nk_font_atlas_clear(&internal->ui.atlas);
}










// Renders the UI context
void OpenGL_UIRender(Renderer* r, void* nk_ctx_void, uint32_t width, uint32_t height)
{
    (void)nk_ctx_void;
    OverlayDrawList draw_list = {0};

    if (UI_BuildDrawList(&draw_list))
        OpenGL_DrawOverlay(r, &draw_list, width, height);
    
    OverlayDrawList_Free(&draw_list);
}