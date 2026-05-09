#include "audio.h"
#include "../core/log.h"
#include <stdlib.h>
#include <string.h>

#define MINIAUDIO_IMPLEMENTATION
#include "../include/miniaudio.h"



typedef struct AudioClip
{
    ma_sound sound;
    bool active;
} AudioClip;



typedef struct AudioBackend
{
    ma_engine engine;
    AudioClip clip_pool[MAX_AUDIO_CLIPS];
} AudioBackend;



static AudioBackend* backend = NULL;



// --- Core ---

// Initializes miniaudio engine
bool Audio_Init()
{
    backend = malloc(sizeof(AudioBackend));
    if (!backend) return false;

    memset(backend, 0, sizeof(AudioBackend));

    // Initialize the Miniaudio Engine
    ma_result result = ma_engine_init(NULL, &backend->engine);
    if (result != MA_SUCCESS)
    {
        Log_Error("Failed to initialize audio engine");
        free(backend);
        return false;
    }

    Log_Info("Audio engine initialized successfully.");
    return true;
}





// Shutsdown miniaudio system
void Audio_Shutdown()
{
    if (!backend) return;

    // Clean up all loaded sounds
    for (uint32_t i = 1; i < MAX_AUDIO_CLIPS; i++)
    {
        if (backend->clip_pool[i].active)
        {
            Audio_DestroyClip((AudioClipHandle){i});
        }
    }

    // Shut down the core engine
    ma_engine_uninit(&backend->engine);
    free(backend);

    backend = NULL;
}





// Sets the global volume for the audio engine
void Audio_SetMasterVolume(float volume)
{
    if (!backend) return;

    // Safety clamp to ensure we don't set the volume too loud
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    // This scales the volume of the entire miniaudio mixing engine at once
    ma_engine_set_volume(&backend->engine, volume);
}





// Stops playing an audio clip
void Audio_Stop(AudioClipHandle clip)
{
    if (!backend || clip.id == 0 || clip.id >= MAX_AUDIO_CLIPS)
        return;

    AudioClip* audio = &backend->clip_pool[clip.id];
    if (audio->active)
        ma_sound_stop(&audio->sound);
}





// Stops all audio playing
void Audio_StopAll(void)
{
    if (!backend) return;

    // Loop through our entire managed pool
    for (uint32_t i = 1; i < MAX_AUDIO_CLIPS; i++) 
    {
        AudioClip* audio = &backend->clip_pool[i];
        
        // If the clip exists and is currently playing, stop it
        if (audio->active && ma_sound_is_playing(&audio->sound)) 
        {
            ma_sound_stop(&audio->sound);
            
            // rewind the clip to the beginning so if the user calls Play() later it doesn't resume
            ma_sound_seek_to_pcm_frame(&audio->sound, 0); 
        }
    }
}





// Returns if an audio clip is currently playing or not
bool Audio_IsPlaying(AudioClipHandle clip)
{
    if (!backend || clip.id == 0 || clip.id >= MAX_AUDIO_CLIPS)
        return false;

    AudioClip* audio = &backend->clip_pool[clip.id];
    if (audio->active)
        return ma_sound_is_playing(&audio->sound);

    return false;
}





// Loads an audio clip into memory
AudioClipHandle Audio_LoadClip(const char* filepath)
{
    if (!backend) return (AudioClipHandle){0};

    // Find a free slot
    uint32_t id = 0;
    for (uint32_t i = 1; i < MAX_AUDIO_CLIPS; i++)
    {
        if (!backend->clip_pool[i].active)
        {
            id = i;
            break;
        }
    }

    if (id == 0)
    {
        Log_Warning("Audio clip pool is full!");
        return (AudioClipHandle){0};
    }

    // Load the sound file into memory using Miniaudio
    // MA_SOUND_FLAG_DECODE flags it to load fully into RAM.
    ma_result result = ma_sound_init_from_file(
        &backend->engine, 
        filepath, 
        MA_SOUND_FLAG_DECODE, 
        NULL, 
        NULL, 
        &backend->clip_pool[id].sound
    );

    if (result != MA_SUCCESS)
    {
        Log_Error("Failed to load audio clip: %s", filepath);
        return (AudioClipHandle){0};
    }

    backend->clip_pool[id].active = true;

    return (AudioClipHandle){id};
}





// Deletes an audio clip from memory
void Audio_DestroyClip(AudioClipHandle clip)
{
    if (!backend || clip.id == 0 || clip.id >= MAX_AUDIO_CLIPS) return;

    AudioClip* audio = &backend->clip_pool[clip.id];
    if (audio->active)
    {
        ma_sound_uninit(&audio->sound);
        audio->active = false;
    }
}





// Plays an audio clip
void Audio_Play(AudioClipHandle clip, float volume, bool loop)
{
    if (!backend || clip.id == 0 || clip.id >= MAX_AUDIO_CLIPS) return;

    AudioClip* audio = &backend->clip_pool[clip.id];
    if (audio->active)
    {
        // Reset the sound to the beginning, set parameters, and play!
        ma_sound_seek_to_pcm_frame(&audio->sound, 0);
        ma_sound_set_volume(&audio->sound, volume);
        ma_sound_set_looping(&audio->sound, loop);
        ma_sound_start(&audio->sound);
    }
}





// Sets the listener to a specific position and direction
void Audio_SetListenerPosition(Vector3 position, Vector3 forward, Vector3 up)
{
    if (!backend) return;
    
    // Miniaudio supports multiple listeners, however index 0 is the main one
    ma_engine_listener_set_position(&backend->engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&backend->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&backend->engine, 0, up.x, up.y, up.z);
}





// Sets the position of an audio source
void Audio_SetSourcePosition(AudioClipHandle clip, Vector3 position)
{
    if (!backend || clip.id == 0) return;

    AudioClip* audio = &backend->clip_pool[clip.id];
    if (audio->active)
        ma_sound_set_position(&audio->sound, position.x, position.y, position.z);
}





// Sets the min and max distance for audio sources
void Audio_SetSourceDistances(AudioClipHandle clip, float min_dist, float max_dist)
{
    if (!backend || clip.id == 0) return;

    AudioClip* audio = &backend->clip_pool[clip.id];
    if (audio->active)
    {
        ma_sound_set_min_distance(&audio->sound, min_dist);
        ma_sound_set_max_distance(&audio->sound, max_dist);
    }
}





// Sets an audio source to be spatial or not
void Audio_SetSpatial(AudioClipHandle clip, bool is_spatial)
{
    if (!backend || clip.id == 0) return;
    
    AudioClip* audio = &backend->clip_pool[clip.id];
    if (audio->active)
        ma_sound_set_spatialization_enabled(&audio->sound, is_spatial);
}