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
    
    glGenTextures(1, &internal->ui.font_tex);
    glBindTexture(GL_TEXTURE_2D, internal->ui.font_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    
    nk_font_atlas_end(&internal->ui.atlas, nk_handle_id((int)internal->ui.font_tex), &internal->ui.tex_null);

    // Override the tex_null with the default white texture to guarantee that the UI background shapes sample a solid white pixel (255, 255, 255, 255).
    internal->ui.tex_null.texture = nk_handle_id((int)internal->default_white_texture);
    internal->ui.tex_null.uv = nk_vec2(0.5f, 0.5f);

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
    glDeleteTextures(1, &internal->ui.font_tex);
    nk_font_atlas_clear(&internal->ui.atlas);
}










// Renders the UI context
void OpenGL_UIRender(Renderer* r, void* nk_ctx_void, uint32_t width, uint32_t height)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    struct nk_context* ctx = (struct nk_context*)nk_ctx_void;
    
    GLfloat ortho[4][4] = {
        {2.0f, 0.0f, 0.0f, 0.0f},
        {0.0f,-2.0f, 0.0f, 0.0f},
        {0.0f, 0.0f,-1.0f, 0.0f},
        {-1.0f,1.0f, 0.0f, 1.0f},
    };
    ortho[0][0] /= (GLfloat)width;
    ortho[1][1] /= (GLfloat)height;

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    glUseProgram(internal->shader_pool[internal->ui.shader.id].program);
    glUniform1i(internal->ui.uniform_tex, 0);
    glUniformMatrix4fv(internal->ui.uniform_proj, 1, GL_FALSE, &ortho[0][0]);
    
    glBindVertexArray(internal->ui.vao);
    glBindBuffer(GL_ARRAY_BUFFER, internal->ui.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal->ui.ebo);

    struct nk_convert_config config;
    static const struct nk_draw_vertex_layout_element vertex_layout[] = {
        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_draw_vertex, position)},
        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_draw_vertex, uv)},
        {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_draw_vertex, col)},
        {NK_VERTEX_LAYOUT_END}
    };
    
    memset(&config, 0, sizeof(config));
    config.tex_null = internal->ui.tex_null;
    config.vertex_layout = vertex_layout;
    config.vertex_size = sizeof(struct nk_draw_vertex);
    config.vertex_alignment = NK_ALIGNOF(struct nk_draw_vertex);
    config.global_alpha = 1.0f;
    config.shape_AA = NK_ANTI_ALIASING_ON;
    config.line_AA = NK_ANTI_ALIASING_ON;
    config.circle_segment_count = 22;
    config.curve_segment_count = 22;
    config.arc_segment_count = 22;

    struct nk_buffer cmds, vbuf, ebuf;
    nk_buffer_init_default(&cmds);
    nk_buffer_init_default(&vbuf);
    nk_buffer_init_default(&ebuf);
    nk_convert(ctx, &cmds, &vbuf, &ebuf, &config);

    void *vertices = nk_buffer_memory(&vbuf);
    void *elements = nk_buffer_memory(&ebuf);
    
    glBufferData(GL_ARRAY_BUFFER, vbuf.allocated, vertices, GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ebuf.allocated, elements, GL_STREAM_DRAW);
    
    const struct nk_draw_command *cmd;
    const nk_draw_index *offset = NULL;
    
    nk_draw_foreach(cmd, ctx, &cmds) {
        if (!cmd->elem_count)
            continue;
        glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
        glScissor(
            (GLint)(cmd->clip_rect.x),
            (GLint)((height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h))),
            (GLint)(cmd->clip_rect.w),
            (GLint)(cmd->clip_rect.h)
        );
        glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
        offset += cmd->elem_count;
    }
    nk_clear(ctx);
    nk_buffer_free(&cmds);
    nk_buffer_free(&vbuf);
    nk_buffer_free(&ebuf);

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}










// Starts the overlay state in OpenGL
static void OpenGL_BeginOverlayState(OpenGL_Backend* internal, uint32_t width, uint32_t height)
{
    GLfloat ortho[4][4] = {
        {2.0f, 0.0f, 0.0f, 0.0f},
        {0.0f,-2.0f, 0.0f, 0.0f},
        {0.0f, 0.0f,-1.0f, 0.0f},
        {-1.0f,1.0f, 0.0f, 1.0f},
    };
    ortho[0][0] /= (GLfloat)width;
    ortho[1][1] /= (GLfloat)height;

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    glUseProgram(internal->shader_pool[internal->ui.shader.id].program);
    glUniform1i(internal->ui.uniform_tex, 0);
    glUniformMatrix4fv(internal->ui.uniform_proj, 1, GL_FALSE, &ortho[0][0]);

    glBindVertexArray(internal->ui.vao);
    glBindBuffer(GL_ARRAY_BUFFER, internal->ui.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal->ui.ebo);
}










// Ends the overlay state in OpenGL
static void OpenGL_EndOverlayState(void)
{
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}










// Draws all overlays
void OpenGL_DrawOverlay(Renderer* r, const OverlayDrawList* list, uint32_t width, uint32_t height)
{
    if (!r || !list || width == 0 || height == 0)
        return;
    if (list->command_count == 0 || list->index_count == 0)
        return;

    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    OpenGL_BeginOverlayState(internal, width, height);

    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(list->vertex_count * sizeof(OverlayVertex)), list->vertices, GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(list->index_count * sizeof(uint16_t)), list->indices, GL_STREAM_DRAW);

    for (uint32_t i = 0; i < list->command_count; i++)
    {
        const OverlayDrawCmd* cmd = &list->commands[i];
        if (cmd->index_count == 0)
            continue;

        GLuint tex = internal->default_white_texture;
        if (cmd->texture.id != 0 && cmd->texture.id < MAX_RESOURCES && internal->texture_pool[cmd->texture.id].active)
            tex = internal->texture_pool[cmd->texture.id].id;
        glBindTexture(GL_TEXTURE_2D, tex);

        float clip_x = cmd->clip_w > 0.0f ? cmd->clip_x : 0.0f;
        float clip_y = cmd->clip_h > 0.0f ? cmd->clip_y : 0.0f;
        float clip_w = cmd->clip_w > 0.0f ? cmd->clip_w : (float)width;
        float clip_h = cmd->clip_h > 0.0f ? cmd->clip_h : (float)height;

        glScissor(
            (GLint)clip_x,
            (GLint)((float)height - (clip_y + clip_h)),
            (GLint)clip_w,
            (GLint)clip_h
        );

        const void* offset = (const void*)(uintptr_t)(cmd->index_offset * sizeof(uint16_t));
        glDrawElements(GL_TRIANGLES, (GLsizei)cmd->index_count, GL_UNSIGNED_SHORT, offset);
    }

    OpenGL_EndOverlayState();
}