#ifndef ENGINE_RENDER_THREAD_H
#define ENGINE_RENDER_THREAD_H


#include "render/render_frame.h"
#include "platform/platform_core.h"






// Results produced by the render thread for one completed frame
typedef struct RenderFrameResult
{
    uint64_t frame_id;
    void* scene_identity;
    RenderProbeResult probe_results[RENDER_FRAME_MAX_PROBES];
    uint32_t probe_result_count;
} RenderFrameResult;





// Ownership state for one slot in the bounded render frame queue
typedef enum RenderFrameSlotState
{
    RENDER_FRAME_SLOT_FREE = 0,
    RENDER_FRAME_SLOT_WRITING,
    RENDER_FRAME_SLOT_READY,
    RENDER_FRAME_SLOT_RENDERING,
    RENDER_FRAME_SLOT_COMPLETE,
    RENDER_FRAME_SLOT_APPLYING
} RenderFrameSlotState;





// Runtime-owned synchronized handoff between update and render threads
typedef struct RenderFrameQueue
{
    RenderFrame buffers[2];
    RenderFrameResult results[2];
    RenderFrameSlotState slot_states[2];
    uint32_t write_index;
    void* scene_identities[2];
    bool stopping;
    bool wake_requested;
    bool redraw_requested;
    uint32_t redraw_width;
    uint32_t redraw_height;
    PlatformMutex* mutex;
    PlatformCondition* frame_ready;
    PlatformCondition* slot_changed;
} RenderFrameQueue;





// Initializes a bounded two-slot render frame queue
bool RenderFrameQueue_Init(RenderFrameQueue* queue);

// Shuts down a render frame queue after its worker has joined
void RenderFrameQueue_Shutdown(RenderFrameQueue* queue);



// Claims a free frame slot for the update thread
RenderFrame* RenderFrameQueue_BeginWrite(RenderFrameQueue* queue);

// Commits the current write slot and wakes the render thread
bool RenderFrameQueue_CommitWrite(RenderFrameQueue* queue, void* scene_identity);

// Waits until the render thread can claim a submitted frame or redraw
bool RenderFrameQueue_WaitRead(RenderFrameQueue* queue, uint32_t* slot_index, const RenderFrame** frame, bool* is_redraw, uint32_t* output_width, uint32_t* output_height);

// Publishes renderer output for a consumed frame
void RenderFrameQueue_CompleteRead(RenderFrameQueue* queue, uint32_t slot_index, const RenderFrameResult* result);

// Claims the oldest completed result for main-thread application
bool RenderFrameQueue_AcquireCompleted(RenderFrameQueue* queue, bool wait, uint32_t* slot_index, const RenderFrameResult** result);

// Returns an applied completion slot to the producer
void RenderFrameQueue_ReleaseCompleted(RenderFrameQueue* queue, uint32_t slot_index);

// Reports whether a frame slot is immediately available
bool RenderFrameQueue_HasFreeSlot(RenderFrameQueue* queue);

// Reports whether queue shutdown has been requested
bool RenderFrameQueue_IsStopping(RenderFrameQueue* queue);

// Wakes the consumer to process non-frame renderer work
void RenderFrameQueue_Wake(RenderFrameQueue* queue);

// Requests replay of the newest completed frame at a new output size
void RenderFrameQueue_RequestRedraw(RenderFrameQueue* queue, uint32_t width, uint32_t height);

// Stops new queue work and wakes all waiting threads
void RenderFrameQueue_RequestStop(RenderFrameQueue* queue);



struct PrismEngine;

// Starts the render worker and waits until it owns the graphics context
bool EngineRenderThread_Start(struct PrismEngine* engine);

// Stops and joins the render worker after finishing render-side teardown
void EngineRenderThread_Stop(struct PrismEngine* engine);

// Applies one completed render result and releases its frame slot
bool EngineRenderThread_ApplyOneCompletion(struct PrismEngine* engine, bool wait);

// Applies every render completion currently available without blocking
void EngineRenderThread_PumpCompletions(struct PrismEngine* engine);





#endif // ENGINE_RENDER_THREAD_H