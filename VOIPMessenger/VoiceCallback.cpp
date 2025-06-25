#include "VoiceCallback.h"
#include <functional>

VoiceCallback::VoiceCallback(handlerType handler, AudioOutput* pAudioOutput) : methodHandler(handler), audioOutput(pAudioOutput) {
}

void VoiceCallback::OnBufferEnd(void* pBufferContext) {
    std::invoke(methodHandler, audioOutput, pBufferContext);
}