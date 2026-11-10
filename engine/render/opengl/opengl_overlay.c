#include "opengl_internal.h"





// Initializes the overlay in OpenGL
void OpenGL_OverlayInit(Renderer* r)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    GL_OverlayPipeline* overlay = &internal->overlay;
    GLuint program = internal->shader_pool[overlay->shader.id].program;

    overlay->uniform_tex = glGetUniformLocation(program, "Texture");
    overlay->uniform_proj = glGetUniformLocation(program, "ProjMtx");
    overlay->attrib_pos = glGetAttribLocation(program, "Position");
    overlay->attrib_uv = glGetAttribLocation(program, "TexCoord");
    overlay->attrib_col = glGetAttribLocation(program, "Color");

    glGenBuffers(1, &overlay->vbo);
    glGenBuffers(1, &overlay->ebo);
    glGenVertexArrays(1, &overlay->vao);

    glBindVertexArray(overlay->vao);
    glBindBuffer(GL_ARRAY_BUFFER, overlay->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, overlay->ebo);

    glEnableVertexAttribArray((GLuint)overlay->attrib_pos);
    glEnableVertexAttribArray((GLuint)overlay->attrib_uv);
    glEnableVertexAttribArray((GLuint)overlay->attrib_col);
    glVertexAttribPointer((GLuint)overlay->attrib_pos, 2, GL_FLOAT, GL_FALSE,
        sizeof(OverlayVertex), (void*)offsetof(OverlayVertex, position));
    glVertexAttribPointer((GLuint)overlay->attrib_uv, 2, GL_FLOAT, GL_FALSE,
        sizeof(OverlayVertex), (void*)offsetof(OverlayVertex, uv));
    glVertexAttribPointer((GLuint)overlay->attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE,
        sizeof(OverlayVertex), (void*)offsetof(OverlayVertex, color));
    glBindVertexArray(0);
}





// Shuts down the overlay in OpenGL
void OpenGL_OverlayShutdown(Renderer* r)
{
    if (!r)
        return;

    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    GL_OverlayPipeline* overlay = &internal->overlay;
    glDeleteBuffers(1, &overlay->vbo);
    glDeleteBuffers(1, &overlay->ebo);
    glDeleteVertexArrays(1, &overlay->vao);

    if (overlay->shader.id != RENDER_INVALID_HANDLE)
        r->DestroyShader(r, overlay->shader);
}





// Begins the overlay state in the OpenGL context
static void OpenGL_BeginOverlayState(OpenGL_Backend* internal, uint32_t width, uint32_t height)
{
    GL_OverlayPipeline* overlay = &internal->overlay;
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

    glUseProgram(internal->shader_pool[overlay->shader.id].program);
    glUniform1i(overlay->uniform_tex, 0);
    glUniformMatrix4fv(overlay->uniform_proj, 1, GL_FALSE, &ortho[0][0]);

    glBindVertexArray(overlay->vao);
    glBindBuffer(GL_ARRAY_BUFFER, overlay->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, overlay->ebo);
}





// Ends the overlay state in the context
static void OpenGL_EndOverlayState()
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
    if (!r || !list || width == 0 || height == 0 || list->command_count == 0 || list->index_count == 0)
        return;

    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    OpenGL_BeginOverlayState(internal, width, height);

    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(list->vertex_count * sizeof(OverlayVertex)), list->vertices, GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(list->index_count * sizeof(uint16_t)), list->indices, GL_STREAM_DRAW);

    for (uint32_t i = 0; i < list->command_count; i++)
    {
        const OverlayDrawCmd* command = &list->commands[i];
        if (command->index_count == 0)
            continue;

        GLuint texture = internal->default_white_texture;
        if (command->texture.id != 0 && command->texture.id < MAX_RESOURCES && internal->texture_pool[command->texture.id].active)
            texture = internal->texture_pool[command->texture.id].id;
        glBindTexture(GL_TEXTURE_2D, texture);

        float clip_x = command->clip_w > 0.0f ? command->clip_x : 0.0f;
        float clip_y = command->clip_h > 0.0f ? command->clip_y : 0.0f;
        float clip_w = command->clip_w > 0.0f ? command->clip_w : (float)width;
        float clip_h = command->clip_h > 0.0f ? command->clip_h : (float)height;
        glScissor((GLint)clip_x, (GLint)((float)height - (clip_y + clip_h)), (GLint)clip_w, (GLint)clip_h);

        const void* offset = (const void*)(uintptr_t)(command->index_offset * sizeof(uint16_t));
        glDrawElements(GL_TRIANGLES, (GLsizei)command->index_count, GL_UNSIGNED_SHORT, offset);
    }

    OpenGL_EndOverlayState();
}
