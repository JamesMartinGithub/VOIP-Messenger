#include "AudioInput.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include "Audioclient.h"
#include "audiopolicy.h"
#include "AudioSessionTypes.h"
#include "functiondiscoverykeys_devpkey.h"
#include "Controller.h"
#include <iostream>
#include <thread>

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { CloseStream(true); }
#define EXIT_ON_ERROR_RETURN(hres)  \
              if (FAILED(hres)) { CloseStream(true); return false; }
#define EXIT_ON_ERROR_RETURN_NAMES(hres, names)  \
              if (FAILED(hres)) { CloseStream(true); return names; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL && (punk) != nullptr)  \
                { (punk)->Release(); (punk) = NULL; }

AudioInput::AudioInput(Controller* pController) 
    : controller(pController) {
}

AudioInput::~AudioInput() {
    applicationClosed = true;
    CloseStream();
}

std::list<std::wstring> AudioInput::GetDeviceNames() {
    std::list<std::wstring> deviceNames;
    HRESULT hr;
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

    // Initialise COM
    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, NULL,
        CLSCTX_ALL, IID_IMMDeviceEnumerator,
        (void**)&deviceEnumerator);
    EXIT_ON_ERROR_RETURN_NAMES(hr, deviceNames);

    // Get all input devices
    deviceEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &deviceCollection);
    UINT endpointCount;
    deviceCollection->GetCount(&endpointCount);
    for (int i = 0; i < endpointCount; i++) {
        hr = deviceCollection->Item(i, &inputDevice);
        EXIT_ON_ERROR_RETURN_NAMES(hr, deviceNames);
        IPropertyStore* store = NULL;
        hr = inputDevice->OpenPropertyStore(STGM_READ, &store);
        EXIT_ON_ERROR_RETURN_NAMES(hr, deviceNames);
        PROPVARIANT deviceName;
        hr = store->GetValue(PKEY_Device_FriendlyName, &deviceName);
        EXIT_ON_ERROR_RETURN_NAMES(hr, deviceNames);
        // Add device name to list
        std::wstring deviceNameStr(deviceName.pwszVal);
        deviceNames.push_back(deviceNameStr);
        SAFE_RELEASE(store)
        SAFE_RELEASE(inputDevice)
    }
    SAFE_RELEASE(deviceCollection)
    SAFE_RELEASE(deviceEnumerator)
    return deviceNames;
}

void AudioInput::SetSelectedDevice(int index) {
    selectedDevice = index;
}

bool AudioInput::Start() {
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
    const IID IID_IAudioClient = __uuidof(IAudioClient);
    const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
    HRESULT hr;
    inputShouldEnd = FALSE;

    // Initialise COM
    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, NULL,
        CLSCTX_ALL, IID_IMMDeviceEnumerator,
        (void**)&deviceEnumerator);
    EXIT_ON_ERROR_RETURN(hr)

    // Get all input devices
    deviceEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &deviceCollection);

    // Get selected input device
    hr = deviceCollection->Item(selectedDevice, &inputDevice);
    // Get default input device
    //hr = deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &inputDevice);
    EXIT_ON_ERROR_RETURN(hr)
    

    hr = inputDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audioClient);
    EXIT_ON_ERROR_RETURN(hr)

    // Get audio format
    WAVEFORMATEX* format = 0;
    hr = audioClient->GetMixFormat(&format);
    EXIT_ON_ERROR_RETURN(hr)
    frameSize = (format->wBitsPerSample * format->nChannels) / 8;

    // Initialise audio client
    hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsRequestedDuration, 0, format, NULL);
    EXIT_ON_ERROR_RETURN(hr)

    // Get the size of the allocated buffer.
    hr = audioClient->GetBufferSize(&bufferFrameCount);
    EXIT_ON_ERROR_RETURN(hr)

    hr = audioClient->GetService(IID_IAudioCaptureClient, (void**)&captureClient);
    EXIT_ON_ERROR_RETURN(hr)

    // Send the audio format to the connected friend
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE formatEx;
        formatEx = *reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format);
        format->wFormatTag = EXTRACT_WAVEFORMATEX_ID(&formatEx.SubFormat);
    }
    controller->SendAudioFormat(format);

    // Calculate the actual duration of the allocated buffer.
    hnsActualDuration = (double)REFTIMES_PER_SEC * bufferFrameCount / format->nSamplesPerSec;

    // Start recording
    hr = audioClient->Start();
    EXIT_ON_ERROR_RETURN(hr)

    // Start buffer read loop in new thread
    std::thread recordThread([this] { RecordAudioStream(); });
    recordThread.detach();
    return true;
}

void AudioInput::Stop() {
    CloseStream();
}

void AudioInput::RecordAudioStream() {
    HRESULT hr;
    UINT32 numFramesAvailable;
    UINT32 packetLength = 0;
    BYTE* pData;
    DWORD flags;

    // Each loop fills about half of the shared buffer.
    while (inputShouldEnd == FALSE) {
        // Sleep for half the buffer duration.
        Sleep(hnsActualDuration / REFTIMES_PER_MILLISEC / 2);
        if (captureClient == nullptr || inputShouldEnd) {
            return;
        }
        hr = captureClient->GetNextPacketSize(&packetLength);
        EXIT_ON_ERROR(hr)
            while (packetLength != 0) {
                // Get the available data in the shared buffer.
                hr = captureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
                EXIT_ON_ERROR(hr)
                if (applicationClosed) {
                    return;
                }
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    pData = NULL;  // Tell CopyData to write silence.
                }
                // Send available capture data to controller
                controller->SendAudioData(pData, numFramesAvailable, numFramesAvailable * frameSize);
                hr = captureClient->ReleaseBuffer(numFramesAvailable);
                EXIT_ON_ERROR(hr)
                hr = captureClient->GetNextPacketSize(&packetLength);
                EXIT_ON_ERROR(hr)
            }
    }
    // Stop recording.
    hr = audioClient->Stop();
    if (FAILED(hr)) {
        CloseStream(true);
    } else {
        CloseStream();
    }
}

void AudioInput::CloseStream(bool failed) {
    inputShouldEnd = TRUE;
    SAFE_RELEASE(deviceCollection)
    SAFE_RELEASE(deviceEnumerator)
    SAFE_RELEASE(inputDevice)
    SAFE_RELEASE(audioClient)
    SAFE_RELEASE(captureClient)
    if (failed) {
        std::cout << "Audio input stopped: ERROR\n";
    }
}
