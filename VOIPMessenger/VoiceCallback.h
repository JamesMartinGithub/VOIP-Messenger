#ifndef VOICECALLBACK_H
#define VOICECALLBACK_H

#include "AudioOutput.h"
#include <xaudio2.h>

class AudioOutput;
struct IXAudio2VoiceCallback;

typedef void (AudioOutput::* handlerType)(void* pBufferContext);

class VoiceCallback : public IXAudio2VoiceCallback {
public:
    VoiceCallback(handlerType handler, AudioOutput* pAudioOutput);
    void OnBufferEnd(void* pBufferContext);

    handlerType methodHandler;
    AudioOutput* audioOutput;

    // Stub methods
    void OnStreamEnd() {}
    void OnVoiceProcessingPassEnd() {}
    void OnVoiceProcessingPassStart(UINT32 SamplesRequired) {}
    void OnBufferStart(void* pBufferContext) {}
    void OnLoopEnd(void* pBufferContext) {}
    void OnVoiceError(void* pBufferContext, HRESULT Error) {}
};

#endif //VOICECALLBACK_H