#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include "Controller.h"
#include "xaudio2.h"
#include <queue>

#define NULL 0

class VoiceCallback;
struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;
typedef struct tWAVEFORMATEX WAVEFORMATEX;
struct XAUDIO2_BUFFER;
typedef unsigned char BYTE;
typedef unsigned int UINT32;

class AudioOutput {
public:
	AudioOutput();
	~AudioOutput();
	bool Start(WAVEFORMATEX* pFormat);
	void Stop(bool failed = false);
	void UpdateBuffer(BYTE* buffer, UINT32 frameNum);
	void FreeBufferData(void* pBufferContext);
	bool IsStarted();

private:

	union Context {
		int id;
		void* ptr;
	};
	IXAudio2* xAudio2 = nullptr;
	IXAudio2MasteringVoice* masteringVoice = nullptr;
	IXAudio2SourceVoice* sourceVoice = nullptr;
	VoiceCallback* voiceCallback = nullptr;
	int frameSize = 0;
	std::queue<BYTE*> packetBuffer;
	int bufferCount = 0;
	bool started = false;
	bool applicationShutdown = false;
};

#endif // AUDIOOUTPUT_H