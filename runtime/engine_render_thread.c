#include "engine_runtime.h"
#include <stdlib.h>
#include <string.h>





typedef void (*EngineRenderThreadWakeFunction)(void* user_data);





// Runtime-owned synchronization state for forwarded renderer commands
typedef struct EngineRenderCommandDispatch
{
    PlatformMutex* mutex;
    PlatformCondition* command_complete;
    EngineRenderThreadWakeFunction wake;
    void* wake_user_data;
    uint64_t render_thread_id;
    RenderCommandType type;
    const void* input;
    void* output;
    bool pending;
    bool stopping;
    uint32_t active_callers;
    RendererSettings cached_settings;
    bool cached_settings_valid;
} EngineRenderCommandDispatch;










// Reports whether the current platform thread owns direct backend access
static bool EngineRenderCommand_HasDirectAccess(void* user_data)
{
    EngineRenderCommandDispatch* dispatch = (EngineRenderCommandDispatch*)user_data;
    if (!dispatch)
        return true;

    Platform_LockMutex(dispatch->mutex);
    bool is_owner = dispatch->render_thread_id == Platform_GetCurrentThreadID();
    Platform_UnlockMutex(dispatch->mutex);
    
    return is_owner;
}










// Forwards one renderer command to the render thread and waits for its result
static bool EngineRenderCommand_Dispatch(void* user_data, RenderCommandType type, const void* input, void* output)
{
    EngineRenderCommandDispatch* dispatch = (EngineRenderCommandDispatch*)user_data;
    if (!dispatch)
        return false;

    Platform_LockMutex(dispatch->mutex);
    
    // Calls already on the render thread may use the backend vtable directly
    if (dispatch->render_thread_id == Platform_GetCurrentThreadID())
    {
        Platform_UnlockMutex(dispatch->mutex);
        return false;
    }
    
    // Settings reads use the synchronized CPU copy instead of stalling on the backend
    if (type == RENDER_COMMAND_GET_SETTINGS && dispatch->cached_settings_valid)
    {
        *(RendererSettings*)output = dispatch->cached_settings;
        Platform_UnlockMutex(dispatch->mutex);
        return true;
    }
    if (type == RENDER_COMMAND_SET_SETTINGS && input)
    {
        dispatch->cached_settings = *(const RendererSettings*)input;
        dispatch->cached_settings_valid = true;
    }
    
    // Only one borrowed command payload may be pending at a time
    dispatch->active_callers++;
    while (dispatch->pending && !dispatch->stopping)
        Platform_WaitCondition(dispatch->command_complete, dispatch->mutex);
    
    if (dispatch->stopping)
    {
        dispatch->active_callers--;
        Platform_BroadcastCondition(dispatch->command_complete);
        Platform_UnlockMutex(dispatch->mutex);
        return true;
    }
    
    // Publish the command and wake the thread that owns the graphics context
    dispatch->type = type;
    dispatch->input = input;
    dispatch->output = output;
    dispatch->pending = true;
    if (dispatch->wake)
        dispatch->wake(dispatch->wake_user_data);
    
    // Keep the caller blocked so its borrowed input and output pointers remain valid
    while (dispatch->pending && !dispatch->stopping)
        Platform_WaitCondition(dispatch->command_complete, dispatch->mutex);
    
    dispatch->active_callers--;
    Platform_BroadcastCondition(dispatch->command_complete);
    Platform_UnlockMutex(dispatch->mutex);
    
    // A stopping dispatcher consumes the call so wrappers cannot touch a foreign context
    return true;
}











// Enables runtime-owned command forwarding for a renderer
static bool EngineRenderCommand_Enable(Renderer* renderer, EngineRenderThreadWakeFunction wake, void* wake_user_data)
{
    if (!renderer || renderer->command_dispatch_data)
        return false;

    EngineRenderCommandDispatch* dispatch = (EngineRenderCommandDispatch*)calloc(1, sizeof(EngineRenderCommandDispatch));
    if (!dispatch)
        return false;
    
    dispatch->mutex = Platform_CreateMutex();
    dispatch->command_complete = Platform_CreateCondition();
    if (!dispatch->mutex || !dispatch->command_complete)
    {
        if (dispatch->command_complete)
            Platform_DestroyCondition(dispatch->command_complete);
        if (dispatch->mutex)
            Platform_DestroyMutex(dispatch->mutex);
        
        free(dispatch);
        return false;
    }
    
    dispatch->wake = wake;
    dispatch->wake_user_data = wake_user_data;
    if (renderer->GetSettings)
    {
        dispatch->cached_settings = renderer->GetSettings(renderer);
        dispatch->cached_settings_valid = true;
    }

    Render_SetCommandDispatch(renderer, EngineRenderCommand_Dispatch, EngineRenderCommand_HasDirectAccess, dispatch);
    return true;
}










// Records which platform thread exclusively owns backend rendering calls
static void EngineRenderCommand_SetOwner(Renderer* renderer, uint64_t thread_id)
{
    if (!renderer || !renderer->command_dispatch_data)
        return;

    EngineRenderCommandDispatch* dispatch = (EngineRenderCommandDispatch*)renderer->command_dispatch_data;
    
    Platform_LockMutex(dispatch->mutex);
    dispatch->render_thread_id = thread_id;
    Platform_UnlockMutex(dispatch->mutex);
}










// Executes one pending forwarded renderer command on the render thread
static bool EngineRenderCommand_ProcessPending(Renderer* renderer)
{
    if (!renderer || !renderer->command_dispatch_data)
        return false;

    EngineRenderCommandDispatch* dispatch = (EngineRenderCommandDispatch*)renderer->command_dispatch_data;

    Platform_LockMutex(dispatch->mutex);
    if (!dispatch->pending)
    {
        Platform_UnlockMutex(dispatch->mutex);
        return false;
    }
    
    RenderCommandType type = dispatch->type;
    const void* input = dispatch->input;
    void* output = dispatch->output;
    bool refresh_settings = type == RENDER_COMMAND_SET_SETTINGS;
    Platform_UnlockMutex(dispatch->mutex);
    
    // Execute directly through the backend vtable; wrapper calls would recurse.
    switch (type)
    {
        case RENDER_COMMAND_CREATE_MESH:
            *(MeshHandle*)output = renderer->CreateMesh(renderer, (const RenderMeshDesc*)input);
            break;
        case RENDER_COMMAND_UPDATE_MESH:
        {
            const RenderMeshUpdateCommand* command = (const RenderMeshUpdateCommand*)input;
            renderer->UpdateMesh(renderer, command->handle, command->update);
            break;
        }
        case RENDER_COMMAND_DESTROY_MESH:
            renderer->DestroyMesh(renderer, *(const MeshHandle*)input);
            break;


        case RENDER_COMMAND_CREATE_TEXTURE:
            *(TextureHandle*)output = renderer->CreateTexture(renderer, (const RenderTextureDesc*)input);
            break;
        case RENDER_COMMAND_DESTROY_TEXTURE:
            renderer->DestroyTexture(renderer, *(const TextureHandle*)input);
            break;


        case RENDER_COMMAND_CREATE_SHADER:
            *(ShaderHandle*)output = renderer->CreateShader(renderer, (const RenderShaderDesc*)input);
            break;
        case RENDER_COMMAND_DESTROY_SHADER:
            renderer->DestroyShader(renderer, *(const ShaderHandle*)input);
            break;


        case RENDER_COMMAND_CREATE_MATERIAL:
            *(MaterialHandle*)output = renderer->CreateMaterial(renderer, (const RenderMaterialDesc*)input);
            break;
        case RENDER_COMMAND_UPDATE_MATERIAL:
        {
            const RenderMaterialUpdateCommand* command = (const RenderMaterialUpdateCommand*)input;
            renderer->UpdateMaterial(renderer, command->handle, command->desc);
            break;
        }
        case RENDER_COMMAND_DESTROY_MATERIAL:
            renderer->DestroyMaterial(renderer, *(const MaterialHandle*)input);
            break;


        case RENDER_COMMAND_CREATE_ENVIRONMENT:
            *(EnvironmentMapHandle*)output =
                renderer->CreateEnvironmentMap(renderer, (const RenderEnvironmentMapDesc*)input);
            break;
        case RENDER_COMMAND_DESTROY_ENVIRONMENT:
            renderer->DestroyEnvironmentMap(renderer, *(const EnvironmentMapHandle*)input);
            break;


        case RENDER_COMMAND_RESIZE:
        {
            const RenderResizeCommand* command = (const RenderResizeCommand*)input;
            renderer->Resize(renderer, command->width, command->height);
            break;
        }
        case RENDER_COMMAND_SET_SETTINGS:
            renderer->SetSettings(renderer, (const RendererSettings*)input);
            break;
        case RENDER_COMMAND_GET_SETTINGS:
            *(RendererSettings*)output = renderer->GetSettings(renderer);
            break;
        case RENDER_COMMAND_SET_VSYNC:
            renderer->SetVSync(renderer, *(const bool*)input);
            break;
        case RENDER_COMMAND_GET_PROBE_RESULTS:
        {
            const RenderProbeResultsCommand* command = (const RenderProbeResultsCommand*)input;
            *(uint32_t*)output =
                renderer->GetProbeResults(renderer, command->out, command->max_count);
            break;
        }
    }

    // Publish any result and release the waiting caller.
    Platform_LockMutex(dispatch->mutex);
    if (refresh_settings && renderer->GetSettings)
    {
        dispatch->cached_settings = renderer->GetSettings(renderer);
        dispatch->cached_settings_valid = true;
    }
    
    dispatch->pending = false;
    dispatch->input = NULL;
    dispatch->output = NULL;
    Platform_BroadcastCondition(dispatch->command_complete);
    Platform_UnlockMutex(dispatch->mutex);
    
    return true;
}










// Stops command forwarding and releases its runtime synchronization state
static void EngineRenderCommand_Disable(Renderer* renderer)
{
    if (!renderer || !renderer->command_dispatch_data)
        return;

    EngineRenderCommandDispatch* dispatch = (EngineRenderCommandDispatch*)renderer->command_dispatch_data;
    
    Platform_LockMutex(dispatch->mutex);
    dispatch->stopping = true;
    Platform_BroadcastCondition(dispatch->command_complete);
    while (dispatch->active_callers > 0)
        Platform_WaitCondition(dispatch->command_complete, dispatch->mutex);
    Platform_UnlockMutex(dispatch->mutex);
    
    Render_ClearCommandDispatch(renderer);
    Platform_DestroyCondition(dispatch->command_complete);
    Platform_DestroyMutex(dispatch->mutex);
    
    free(dispatch);
}










// Finds the first frame slot in a requested ownership state
static int RenderFrameQueue_FindState(const RenderFrameQueue* queue, RenderFrameSlotState state)
{
    for (uint32_t i = 0; i < 2; i++)
    {
        if (queue->slot_states[i] == state)
            return (int)i;
    }

    return -1;
}










// Finds the newest completed frame for modal redraw requests
static int RenderFrameQueue_FindNewestComplete(const RenderFrameQueue* queue)
{
    int newest = -1;
    for (uint32_t i = 0; i < 2; i++)
    {
        if (queue->slot_states[i] != RENDER_FRAME_SLOT_COMPLETE)
            continue;
        
        if (newest < 0 || queue->buffers[i].frame_id > queue->buffers[newest].frame_id)
            newest = (int)i;
    }

    return newest;
}










// Finds the oldest unapplied completion to preserve result ordering
static int RenderFrameQueue_FindOldestComplete(const RenderFrameQueue* queue)
{
    int oldest = -1;
    for (uint32_t i = 0; i < 2; i++)
    {
        if (queue->slot_states[i] != RENDER_FRAME_SLOT_COMPLETE)
            continue;

        if (oldest < 0 || queue->results[i].frame_id < queue->results[oldest].frame_id)
            oldest = (int)i;
    }

    return oldest;
}










// Initializes the bounded two-slot frame queue
bool RenderFrameQueue_Init(RenderFrameQueue* queue)
{
    if (!queue)
        return false;

    memset(queue, 0, sizeof(RenderFrameQueue));
    queue->mutex = Platform_CreateMutex();
    queue->frame_ready = Platform_CreateCondition();
    queue->slot_changed = Platform_CreateCondition();
    
    if (!queue->mutex || !queue->frame_ready || !queue->slot_changed)
    {
        RenderFrameQueue_Shutdown(queue);
        return false;
    }
    
    queue->slot_states[0] = RENDER_FRAME_SLOT_FREE;
    queue->slot_states[1] = RENDER_FRAME_SLOT_FREE;
    
    return true;
}










// Shuts down a render frame queue after its worker has joined
void RenderFrameQueue_Shutdown(RenderFrameQueue* queue)
{
    if (!queue)
        return;

    RenderFrameQueue_RequestStop(queue);
    OverlayDrawList_Free(&queue->buffers[0].retained_ui);
    OverlayDrawList_Free(&queue->buffers[0].immediate_ui);
    OverlayDrawList_Free(&queue->buffers[1].retained_ui);
    OverlayDrawList_Free(&queue->buffers[1].immediate_ui);
    
    if (queue->slot_changed)
        Platform_DestroyCondition(queue->slot_changed);
    if (queue->frame_ready)
        Platform_DestroyCondition(queue->frame_ready);
    if (queue->mutex)
        Platform_DestroyMutex(queue->mutex);
    
    queue->slot_changed = NULL;
    queue->frame_ready = NULL;
    queue->mutex = NULL;
}










// Claims a free slot, blocking to provide bounded back-pressure
RenderFrame* RenderFrameQueue_BeginWrite(RenderFrameQueue* queue)
{
    if (!queue || !queue->mutex)
        return NULL;

    Platform_LockMutex(queue->mutex);
    
    // Wait until the consumer releases one of the two bounded frame slots.
    int slot = RenderFrameQueue_FindState(queue, RENDER_FRAME_SLOT_FREE);
    while (slot < 0 && !queue->stopping)
    {
        Platform_WaitCondition(queue->slot_changed, queue->mutex);
        slot = RenderFrameQueue_FindState(queue, RENDER_FRAME_SLOT_FREE);
    }
    
    if (queue->stopping)
    {
        Platform_UnlockMutex(queue->mutex);
        return NULL;
    }
    
    queue->write_index = (uint32_t)slot;
    queue->slot_states[slot] = RENDER_FRAME_SLOT_WRITING;
    Platform_UnlockMutex(queue->mutex);

    return &queue->buffers[slot];
}










// Publishes the slot currently owned by the producer
bool RenderFrameQueue_CommitWrite(RenderFrameQueue* queue, void* scene_identity)
{
    if (!queue || !queue->mutex)
        return false;

    Platform_LockMutex(queue->mutex);
    uint32_t slot = queue->write_index;
    if (queue->stopping || slot >= 2 || queue->slot_states[slot] != RENDER_FRAME_SLOT_WRITING)
    {
        Platform_UnlockMutex(queue->mutex);
        return false;
    }
    
    queue->scene_identities[slot] = scene_identity;
    queue->slot_states[slot] = RENDER_FRAME_SLOT_READY;
    Platform_SignalCondition(queue->frame_ready);
    Platform_UnlockMutex(queue->mutex);
    
    return true;
}










// Claims the oldest ready frame for the render thread
bool RenderFrameQueue_WaitRead(RenderFrameQueue* queue, uint32_t* slot_index, const RenderFrame** frame, bool* is_redraw, uint32_t* output_width, uint32_t* output_height)
{
    if (!queue || !slot_index || !frame || !is_redraw || !output_width || !output_height || !queue->mutex)
        return false;

    Platform_LockMutex(queue->mutex);
    
    // Prefer newly submitted frames; modal redraws only replay completed data.
    int slot = RenderFrameQueue_FindState(queue, RENDER_FRAME_SLOT_READY);
    int redraw_slot = queue->redraw_requested ? RenderFrameQueue_FindNewestComplete(queue) : -1;
    while (slot < 0 && redraw_slot < 0 && !queue->stopping && !queue->wake_requested)
    {
        Platform_WaitCondition(queue->frame_ready, queue->mutex);
        slot = RenderFrameQueue_FindState(queue, RENDER_FRAME_SLOT_READY);
        redraw_slot = queue->redraw_requested ? RenderFrameQueue_FindNewestComplete(queue) : -1;
    }
    
    if (slot < 0 && redraw_slot >= 0)
    {
        slot = redraw_slot;
        queue->redraw_requested = false;
        *is_redraw = true;
        *output_width = queue->redraw_width;
        *output_height = queue->redraw_height;
    }
    else
    {
        *is_redraw = false;
    }

    if (slot < 0)
    {
        queue->wake_requested = false;
        Platform_UnlockMutex(queue->mutex);
        return false;
    }
    
    // When both slots are ready, preserve frame-id ordering.
    int other = slot == 0 ? 1 : 0;
    if (queue->slot_states[other] == RENDER_FRAME_SLOT_READY && queue->buffers[other].frame_id < queue->buffers[slot].frame_id)
        slot = other;

    queue->slot_states[slot] = RENDER_FRAME_SLOT_RENDERING;
    *slot_index = (uint32_t)slot;
    *frame = &queue->buffers[slot];
    if (!*is_redraw)
    {
        *output_width = queue->buffers[slot].width;
        *output_height = queue->buffers[slot].height;
    }

    Platform_UnlockMutex(queue->mutex);
    
    return true;
}










// Publishes the render result and leaves the frame immutable
void RenderFrameQueue_CompleteRead(RenderFrameQueue* queue, uint32_t slot_index, const RenderFrameResult* result)
{
    if (!queue || !result || slot_index >= 2 || !queue->mutex)
        return;

    Platform_LockMutex(queue->mutex);
    if (queue->slot_states[slot_index] == RENDER_FRAME_SLOT_RENDERING)
    {
        queue->results[slot_index] = *result;
        queue->results[slot_index].scene_identity = queue->scene_identities[slot_index];
        queue->slot_states[slot_index] = RENDER_FRAME_SLOT_COMPLETE;
        Platform_BroadcastCondition(queue->slot_changed);
        if (queue->redraw_requested)
            Platform_SignalCondition(queue->frame_ready);
    }
    
    Platform_UnlockMutex(queue->mutex);
}










// Claims a completed result for the main thread
bool RenderFrameQueue_AcquireCompleted(RenderFrameQueue* queue, bool wait, uint32_t* slot_index, const RenderFrameResult** result)
{
    if (!queue || !slot_index || !result || !queue->mutex)
        return false;

    Platform_LockMutex(queue->mutex);
    int slot = RenderFrameQueue_FindOldestComplete(queue);
    while (wait && slot < 0 && !queue->stopping)
    {
        Platform_WaitCondition(queue->slot_changed, queue->mutex);
        slot = RenderFrameQueue_FindOldestComplete(queue);
    }
    
    if (slot < 0)
    {
        Platform_UnlockMutex(queue->mutex);
        return false;
    }
    
    queue->slot_states[slot] = RENDER_FRAME_SLOT_APPLYING;
    *slot_index = (uint32_t)slot;
    *result = &queue->results[slot];
    Platform_UnlockMutex(queue->mutex);
    
    return true;
}










// Returns an applied completion slot to the producer
void RenderFrameQueue_ReleaseCompleted(RenderFrameQueue* queue, uint32_t slot_index)
{
    if (!queue || slot_index >= 2 || !queue->mutex)
        return;

    Platform_LockMutex(queue->mutex);
    if (queue->slot_states[slot_index] == RENDER_FRAME_SLOT_APPLYING)
    {
        queue->scene_identities[slot_index] = NULL;
        queue->slot_states[slot_index] = RENDER_FRAME_SLOT_FREE;
        Platform_BroadcastCondition(queue->slot_changed);
    }
    Platform_UnlockMutex(queue->mutex);
}










// Reports whether the producer can begin another snapshot without blocking
bool RenderFrameQueue_HasFreeSlot(RenderFrameQueue* queue)
{
    if (!queue || !queue->mutex)
        return false;

    Platform_LockMutex(queue->mutex);
    bool has_free_slot = RenderFrameQueue_FindState(queue, RENDER_FRAME_SLOT_FREE) >= 0;
    Platform_UnlockMutex(queue->mutex);
    
    return has_free_slot;
}










// Reports whether queue shutdown has been requested
bool RenderFrameQueue_IsStopping(RenderFrameQueue* queue)
{
    if (!queue || !queue->mutex)
        return true;

    Platform_LockMutex(queue->mutex);
    bool stopping = queue->stopping;
    Platform_UnlockMutex(queue->mutex);
    
    return stopping;
}










// Wakes the render thread so it can process non-frame renderer commands
void RenderFrameQueue_Wake(RenderFrameQueue* queue)
{
    if (!queue || !queue->mutex)
        return;

    Platform_LockMutex(queue->mutex);
    queue->wake_requested = true;
    Platform_SignalCondition(queue->frame_ready);
    Platform_UnlockMutex(queue->mutex);
}










// Requests replay of the newest completed snapshot at the latest window size
void RenderFrameQueue_RequestRedraw(RenderFrameQueue* queue, uint32_t width, uint32_t height)
{
    if (!queue || !queue->mutex || width == 0 || height == 0)
        return;

    Platform_LockMutex(queue->mutex);
    queue->redraw_requested = true;
    queue->redraw_width = width;
    queue->redraw_height = height;
    Platform_SignalCondition(queue->frame_ready);
    Platform_UnlockMutex(queue->mutex);
}










// Stops new queue work and wakes every blocked producer or consumer
void RenderFrameQueue_RequestStop(RenderFrameQueue* queue)
{
    if (!queue || !queue->mutex)
        return;

    Platform_LockMutex(queue->mutex);
    queue->stopping = true;
    queue->redraw_requested = false;
    Platform_BroadcastCondition(queue->frame_ready);
    Platform_BroadcastCondition(queue->slot_changed);
    Platform_UnlockMutex(queue->mutex);
}










// Wakes the render worker when a forwarded renderer command is available
static void EngineRenderThread_Wake(void* user_data)
{
    PrismEngine* engine = (PrismEngine*)user_data;
    if (engine)
        RenderFrameQueue_Wake(&engine->frame_queue);
}










// Draws a snapshot and adjusts camera projections when replaying it at a new size
static void EngineRenderThread_DrawSnapshot(Renderer* renderer, const RenderFrame* frame, bool is_redraw, uint32_t output_width, uint32_t output_height)
{
    if (!renderer || !frame)
        return;

    if (!is_redraw)
    {
        Render_DrawFrame(renderer, frame);
        return;
    }
    
    RenderLighting lighting;
    RenderFrame_FillLighting(frame, &lighting);
    if (output_height > 0)
        lighting.camera_aspect = (float)output_width / (float)output_height;
    
    // Rebuild only size-dependent view data while reusing the immutable scene snapshot.
    for (uint32_t i = 0; i < frame->view_count; i++)
    {
        const RenderFrameView* source = &frame->views[i];
        RenderWorld world = {
            .view = source->view,
            .lighting = lighting,
            .items = source->item_count > 0 ? &frame->items[source->item_start] : NULL,
            .item_count = source->item_count
        };

        uint32_t view_width = output_width;
        uint32_t view_height = output_height;
        
        if (frame->width > 0 && source->view.window_width > 0)
            view_width = (uint32_t)(((uint64_t)source->view.window_width * output_width) / frame->width);
        if (frame->height > 0 && source->view.window_height > 0)
            view_height = (uint32_t)(((uint64_t)source->view.window_height * output_height) / frame->height);
        if (view_width == 0)
            view_width = 1;
        if (view_height == 0)
            view_height = 1;
        
        world.view.window_width = view_width;
        world.view.window_height = view_height;
        
        if (source->field_of_view > 0.0f && source->near_plane > 0.0f && source->far_plane > source->near_plane)
        {
            float aspect = (float)view_width / (float)view_height;
            world.view.projection_matrix = Matrix4Perspective(source->field_of_view, aspect, source->near_plane, source->far_plane);
        }

        Render_DrawWorld(renderer, &world);
    }
}










// Owns the graphics context and consumes submitted frames and renderer commands
static int EngineRenderThread_Main(void* user_data)
{
    PrismEngine* engine = (PrismEngine*)user_data;
    if (!engine || !engine->renderer)
        return -1;

    // Transfer the renderer context to this worker before accepting any work.
    EngineRenderCommand_SetOwner(engine->renderer, Platform_GetCurrentThreadID());
    bool context_ready = Render_MakeCurrent(engine->renderer);
    
    Platform_LockMutex(engine->render_start_mutex);
    engine->render_thread_ready = context_ready;
    engine->render_thread_failed = !context_ready;
    Platform_BroadcastCondition(engine->render_start_condition);
    Platform_UnlockMutex(engine->render_start_mutex);
    
    if (!context_ready)
        return -1;
    
    // Submitted snapshots take priority so resource changes cannot overtake them.
    while (true)
    {
        uint32_t slot_index = 0;
        const RenderFrame* frame = NULL;
        bool is_redraw = false;
        uint32_t output_width = 0;
        uint32_t output_height = 0;

        if (RenderFrameQueue_WaitRead(&engine->frame_queue, &slot_index, &frame, &is_redraw, &output_width, &output_height))
        {
            RenderFrameResult result = {0};
            result.frame_id = frame->frame_id;

            Render_Resize(engine->renderer, output_width, output_height);

            EngineRenderThread_DrawSnapshot(engine->renderer, frame, is_redraw, output_width, output_height);
            Render_DrawOverlay(engine->renderer, &frame->retained_ui, output_width, output_height);
            Render_DrawOverlay(engine->renderer, &frame->immediate_ui, output_width, output_height);
            result.probe_result_count = Render_GetProbeResults(engine->renderer, result.probe_results, RENDER_FRAME_MAX_PROBES);
            Render_Present(engine->renderer);
            
            RenderFrameQueue_CompleteRead(&engine->frame_queue, slot_index, &result);
            continue;
        }

        // A wake without a frame means a synchronous resource/settings call is waiting.
        while (EngineRenderCommand_ProcessPending(engine->renderer)) { }

        if (RenderFrameQueue_IsStopping(&engine->frame_queue))
            break;
    }

    // GPU-side teardown must happen before this thread releases its context.
    EngineRenderCommand_Disable(engine->renderer);
    Render_Shutdown(engine->renderer);
    
    return 0;
}










// Starts the render worker and waits until it owns the graphics context
bool EngineRenderThread_Start(PrismEngine* engine)
{
    if (!engine || !engine->renderer)
        return false;

    engine->render_start_mutex = Platform_CreateMutex();
    engine->render_start_condition = Platform_CreateCondition();
    if (!engine->render_start_mutex || !engine->render_start_condition)
        return false;
    
    // Install runtime dispatch before the main thread gives up the context.
    if (!EngineRenderCommand_Enable(engine->renderer, EngineRenderThread_Wake, engine))
        return false;
    
    Render_ReleaseCurrent(engine->renderer);
    engine->render_thread = Platform_CreateThread(EngineRenderThread_Main, "PrismRender", engine);
    if (!engine->render_thread)
        return false;

    // Initialization cannot succeed until MakeCurrent has completed on the worker.
    Platform_LockMutex(engine->render_start_mutex);
    while (!engine->render_thread_ready && !engine->render_thread_failed)
        Platform_WaitCondition(engine->render_start_condition, engine->render_start_mutex);
    
    bool ready = engine->render_thread_ready && !engine->render_thread_failed;
    Platform_UnlockMutex(engine->render_start_mutex);
    
    return ready;
}










// Stops and joins the render worker after finishing render-side teardown
void EngineRenderThread_Stop(PrismEngine* engine)
{
    if (!engine)
        return;

    bool worker_owned_renderer = engine->render_thread_ready;
    RenderFrameQueue_RequestStop(&engine->frame_queue);
    if (engine->render_thread)
    {
        Platform_JoinThread(engine->render_thread, NULL);
        engine->render_thread = NULL;
    }
    
    // If startup failed, reclaim and destroy the renderer from the main thread.
    if (engine->renderer && !worker_owned_renderer)
    {
        EngineRenderCommand_Disable(engine->renderer);
        Render_MakeCurrent(engine->renderer);
        Render_Shutdown(engine->renderer);
    }

    engine->renderer = NULL;
    
    if (engine->render_start_condition)
        Platform_DestroyCondition(engine->render_start_condition);
    if (engine->render_start_mutex)
        Platform_DestroyMutex(engine->render_start_mutex);
    
    engine->render_start_condition = NULL;
    engine->render_start_mutex = NULL;
    engine->render_thread_ready = false;
    engine->render_thread_failed = false;
}










// Applies one completed render result and releases its frame slot
bool EngineRenderThread_ApplyOneCompletion(PrismEngine* engine, bool wait)
{
    if (!engine)
        return false;

    uint32_t slot_index = 0;
    const RenderFrameResult* result = NULL;
    if (!RenderFrameQueue_AcquireCompleted(&engine->frame_queue, wait, &slot_index, &result))
        return false;
    
    Scene* result_scene = (Scene*)result->scene_identity;
    if (result_scene && result_scene == engine->active_scene && result->frame_id > engine->last_applied_render_frame)
    {
        Engine_ApplyFrameResults(engine, result_scene, result);
        engine->last_applied_render_frame = result->frame_id;
    }

    RenderFrameQueue_ReleaseCompleted(&engine->frame_queue, slot_index);
    
    return true;
}











// Applies every render completion currently available without blocking
void EngineRenderThread_PumpCompletions(PrismEngine* engine)
{
    while (EngineRenderThread_ApplyOneCompletion(engine, false)) { }
}