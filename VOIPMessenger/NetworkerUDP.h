#ifndef NETWORKERUDP_H
#define NETWORKERUDP_H

#include <string>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <queue>
#include <winsock2.h>

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
	bool Connect(const char* ip);
	void Disconnect();
	bool SendData(char* message, int size);

private:
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
	bool connected = false;
	char messageToSend[512];
	std::queue<char*> dataBuffer;
	std::queue<int> dataSizeBuffer;
	int bufferCount = 0;
	int messageSize = 0;
	std::condition_variable sendCv;
	std::mutex mutex;
	bool sendShouldWake = false;
	bool lastDisconnectLocal = false;
	bool applicationClosed = false;
};

#endif // NETWORKERUDP_H