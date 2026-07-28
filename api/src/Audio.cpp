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

    void AudioClip::Play(float volume, bool loop) {
        // Reconstruct the C-handle and pass it down
        ::AudioClipHandle raw_handle = { m_HandleID };
        ::Audio_Play(raw_handle, volume, loop);
    }

    void AudioClip::Stop() {
        ::AudioClipHandle raw_handle = { m_HandleID };
        ::Audio_Stop(raw_handle);
    }

    bool AudioClip::IsPlaying() {
        ::AudioClipHandle raw_handle = { m_HandleID };
        return ::Audio_IsPlaying(raw_handle);
    }



    void AudioClip::SetSourcePosition(Prism::Vector3 position) {
        ::AudioClipHandle raw_handle = { m_HandleID };
        ::Audio_SetSourcePosition(raw_handle, {position.x, position.y, position.z});
    }

    void AudioClip::SetSourceDistances(float min_dist, float max_dist) {
        ::AudioClipHandle raw_handle = { m_HandleID };
        ::Audio_SetSourceDistances(raw_handle, min_dist, max_dist);
    }

    void AudioClip::SetSpatial(bool is_spatial) {
        ::AudioClipHandle raw_handle = { m_HandleID };
        ::Audio_SetSpatial(raw_handle, is_spatial);
    }



    // ==========================================
    // Global Audio System Implementation
    // ==========================================
    
    void Audio::SetMasterVolume(float volume) {
        ::Audio_SetMasterVolume(volume);
    }

    void Audio::StopAll() {
        ::Audio_StopAll();
    }

    void Audio::SetListenerPosition(Prism::Vector3 position, Prism::Vector3 forward, Prism::Vector3 up) {
        ::Audio_SetListenerPosition({position.x, position.y, position.z}, {forward.x, forward.y, forward.z}, {up.x, up.y, up.z});
    }
}