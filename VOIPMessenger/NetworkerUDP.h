#ifndef NETWORKERUDP_H
#define NETWORKERUDP_H

#include <string>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <MPMCQueue/include/rigtorp/MPMCQueue.h>

class Controller;
struct sockaddr;
struct sockaddr_in;

class NetworkerUDP {
public:
	NetworkerUDP(Controller* pController);
	~NetworkerUDP();
	void SetServer(bool pIsServer);
	void SetSourcePort(unsigned short portNum);
	void SetDestinationAddress(sockaddr* address);
	bool IsConnected();
	bool Connect(const char* ip);
	void Disconnect();
	void SendData(char* message, int size);
	void NotifySendThread();

private:
	struct Data {
		char* data;
		int size;

		Data(char* dataIn, int sizeIn)
			: data(dataIn), size(sizeIn) {
		}

		~Data() {
			delete data;
		}
	};

	void CancelInitialise(std::string disconnectMessage);
	void ThreadReceive();
	void ThreadSend();

	Controller* controller;
	unsigned long long hostSocket;
	unsigned short sourcePort;
	sockaddr_in dest;
	int destSize;
	int destWhenServerSize;
	bool isServer = false;
	std::atomic<bool> connected = false;
	char messageToSend[512];
	rigtorp::MPMCQueue<Data*> sendDataBuffer;
	std::condition_variable sendCv;
	std::mutex mutex;
	bool applicationClosed = false;
};

#endif // NETWORKERUDP_H