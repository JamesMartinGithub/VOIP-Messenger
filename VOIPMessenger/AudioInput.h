#ifndef AUDIOINPUT_H
#define AUDIOINPUT_H

#include <string>
#include <list>
#include <atomic>

class Controller;
struct IMMDeviceCollection;
struct IMMDeviceEnumerator;
struct IMMDevice;
struct IAudioClient;
struct IAudioCaptureClient;

#define NULL 0
#define FALSE 0
typedef int BOOL;
typedef __int64  REFERENCE_TIME;
typedef unsigned int UINT32;
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

class AudioInput {
public:
	AudioInput(Controller* pController);
	~AudioInput();
	std::list<std::wstring> GetDeviceNames();
	void SetSelectedDevice(int index);
	bool Start();
	void Stop();

private:
	void RecordAudioStream();
	void CloseStream(bool failed = false);

	Controller* controller;
	std::atomic<BOOL> inputShouldEnd = FALSE;
	IMMDeviceCollection* deviceCollection = NULL;
	IMMDeviceEnumerator* deviceEnumerator = NULL;
	IMMDevice* inputDevice = NULL;
	IAudioClient* audioClient = NULL;
	IAudioCaptureClient* captureClient = NULL;
	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME hnsActualDuration;
	UINT32 bufferFrameCount;
	int frameSize = 0;
	int selectedDevice = 0;
	bool applicationClosed = false;
};

#endif // AUDIOINPUT_H