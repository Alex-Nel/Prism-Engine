#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "../core/math_core.h" // For Vector3 to implement 3D audio later



// Opaque handle for an audio file loaded in memory
typedef struct { uint32_t id; } AudioClipHandle;


#define AUDIO_INVALID_HANDLE 0
#define MAX_AUDIO_CLIPS 1024


// --- Core ---

bool Audio_Init(void);
void Audio_Shutdown(void);



// --- Global Controls ---

void Audio_SetMasterVolume(float volume); // 0.0f to 1.0f
void Audio_Stop(AudioClipHandle clip);
void Audio_StopAll();
bool Audio_IsPlaying(AudioClipHandle clip);



// --- Resource Management ---

AudioClipHandle Audio_LoadClip(const char* filepath);
void Audio_DestroyClip(AudioClipHandle clip);



// --- Playback ---

// Plays a sound effect once or in a loop. Returns a unique ID for that playing instance if you need to stop it early.
void Audio_Play(AudioClipHandle clip, float volume, bool loop);


// Listener

void Audio_SetListenerPosition(Vector3 position, Vector3 forward, Vector3 up);


// Source

void Audio_SetSourcePosition(AudioClipHandle clip, Vector3 position);
void Audio_SetSourceDistances(AudioClipHandle clip, float min_dist, float max_dist);
void Audio_SetSpatial(AudioClipHandle clip, bool is_spatial);





#endif