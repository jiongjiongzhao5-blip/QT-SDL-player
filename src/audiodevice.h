#ifndef AUDIODEVICE_H
#define AUDIODEVICE_H

#include <cstdint>
#include <functional>

#include <SDL3/SDL_audio.h>

class AudioDevice
{
public:
    using PullCallback = std::function<int(uint8_t* dst, int bytes)>;

    AudioDevice() = default;
    ~AudioDevice();

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    bool open(int sampleRate, int channels, PullCallback pull);

    void close();

    bool isOpen() const { return stream_ != nullptr; }

    void setPaused(bool paused);
    void setVolume(float linear01);
    void setSpeed(float ratio);
    float speed() const { return speed_; }

    int sampleRate() const { return spec_.freq; }
    int channels() const { return spec_.channels; }
    int frameBytes() const;
    int bytesPerSecond() const;

private:
    static void SDLCALL onPull(void* userdata, SDL_AudioStream* stream,
                               int additionalAmount, int totalAmount);

    SDL_AudioStream* stream_ = nullptr;
    SDL_AudioSpec    spec_{};
    PullCallback     pull_;
    float            speed_  = 1.0f;
};

#endif
