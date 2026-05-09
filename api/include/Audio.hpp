#pragma once

#include <string>
#include <cstdint>



namespace Prism 
{
    // --- The Object Wrapper ---

    class AudioClip
    {
    private:
        uint32_t m_HandleID; // The backend handle

    public:
        // Constructor used by the AssetManager to give us the ID
        AudioClip(uint32_t id = 0) : m_HandleID(id) {}

        void Play(float volume = 1.0f, bool loop = false);
        void Stop();
        bool IsPlaying();
        
        // Expose the raw ID just in case the engine needs it internally
        uint32_t GetID() const { return m_HandleID; }
    };



    // --- Global Audio System ---
    
    class Audio 
    {
    public:
        static void SetMasterVolume(float volume);
        static void StopAll();
    };
}