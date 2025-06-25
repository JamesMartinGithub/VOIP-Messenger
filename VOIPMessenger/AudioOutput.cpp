#include "AudioOutput.h"
#include <combaseapi.h>
#include <iostream>
#include "VoiceCallback.h"

AudioOutput::AudioOutput() {
}

AudioOutput::~AudioOutput() {
    applicationShutdown = true;
    started = false;
    if (sourceVoice != nullptr && sourceVoice != NULL) {
        sourceVoice->Stop();
        sourceVoice->DestroyVoice();
        sourceVoice = NULL;
    }
    // Stop XAudio2 engine
    if (xAudio2 != nullptr && xAudio2 != NULL) {
        xAudio2->StopEngine();
        xAudio2->Release();
        xAudio2 = NULL;
    }
    while (bufferCount > 0) {
        delete[] packetBuffer.front();
        packetBuffer.pop();
        bufferCount--;
    }
}

bool AudioOutput::Start(WAVEFORMATEX* format) {
    frameSize = (format->wBitsPerSample * format->nChannels) / 8;

    // Initialise COM
    if (HRESULT result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); result < 0) {
        std::cout << "Unable to initialise COM for audio output: " << result << '\n';
        return false;
    }

    // Create XAudio2 engine
    if (HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR); result < 0) {
        std::cout << "Unable to start XAudio2 engine for audio output" << result << '\n';
        Stop(true);
        return false;
    }

    // Create mastering voice
    if (HRESULT result = xAudio2->CreateMasteringVoice(&masteringVoice, format->nChannels, format->nSamplesPerSec); result < 0) {
        std::cout << "Unable to create XAudio2 mastering voice" << result << '\n';
        Stop(true);
        return false;
    }
    masteringVoice->SetVolume(0.5f);

    // Create source voice
    voiceCallback = new VoiceCallback(&AudioOutput::FreeBufferData, this);
    if (HRESULT result = xAudio2->CreateSourceVoice(&sourceVoice, format, 0, 2.0f, voiceCallback); result < 0) {
        std::cout << "Unable to create XAudio2 source voice" << result << '\n';
        Stop(true);
        return false;
    }

    // Enable buffer playback
    if (HRESULT result = sourceVoice->Start(0); result < 0) {
        std::cout << "Unable to start XAudio2 source voice" << result << '\n';
        Stop(true);
        return false;
    }
    started = true;
    return true;
}

void AudioOutput::Stop(bool failed) {
    started = false;
    if (sourceVoice != nullptr && sourceVoice != NULL) {
        sourceVoice->Stop();
        sourceVoice->DestroyVoice();
        sourceVoice = NULL;
    }
    // Stop XAudio2 engine
    if (xAudio2 != nullptr && xAudio2 != NULL) {
        xAudio2->StopEngine();
        xAudio2->Release();
        xAudio2 = NULL;
    }
    if (failed) {
        std::cout << "Audio output stopped: ERROR\n";
    } else {
        std::cout << "Audio output stopped\n";
    }
}

void AudioOutput::UpdateBuffer(BYTE* buffer, UINT32 frameNum) {
    if (!applicationShutdown) {
        BYTE* bufferData = new BYTE[frameNum * frameSize];
        memcpy(bufferData, buffer, frameNum * frameSize);
        XAUDIO2_BUFFER newBuffer = { 0, frameNum * frameSize, bufferData, 0, 0, 0, 0, 0, nullptr };
        packetBuffer.push(bufferData);
        bufferCount++;
        sourceVoice->SubmitSourceBuffer(&newBuffer);
    }
}

void AudioOutput::FreeBufferData(void* pBufferContext) {
    if (bufferCount > 0 && !applicationShutdown) {
        delete[] packetBuffer.front();
        packetBuffer.pop();
        bufferCount--;
    }
}

bool AudioOutput::IsStarted() {
    return started;
}
