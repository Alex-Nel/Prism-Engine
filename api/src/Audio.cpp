#include "../include/Audio.hpp"

extern "C"
{
    #include "../../audio/audio.h" 
}

namespace Prism 
{
    // ==========================================
    // AudioClip Class Implementation
    // ==========================================

    void AudioClip::Play(float volume, bool loop) 
    {
        // Reconstruct the C-handle and pass it down
        ::AudioClipHandle raw_handle = { m_HandleID };
        ::Audio_Play(raw_handle, volume, loop);
    }



    void AudioClip::Stop() 
    {
        ::AudioClipHandle raw_handle = { m_HandleID };
        ::Audio_Stop(raw_handle);
    }



    bool AudioClip::IsPlaying() 
    {
        ::AudioClipHandle raw_handle = { m_HandleID };
        return ::Audio_IsPlaying(raw_handle);
    }



    // ==========================================
    // Global Audio System Implementation
    // ==========================================
    
    void Audio::SetMasterVolume(float volume) 
    {
        ::Audio_SetMasterVolume(volume);
    }

    void Audio::StopAll() 
    {
        ::Audio_StopAll();
    }
}