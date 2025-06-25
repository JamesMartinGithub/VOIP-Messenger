#ifndef NETWORKERTCP_H
#define NETWORKERTCP_H

#include <string>
#include <condition_variable>
#include <mutex>
#include <thread>

class Controller;

class NetworkerTCP {
public:
	NetworkerTCP(Controller* pController);
	~NetworkerTCP();
	bool ToggleServer();
	bool Connect(const char* ip, const char* port);
	void Disconnect();
	bool SendData(const char* message, int size);

private:
	void CancelInitialise(std::string disconnectMessage);
	void ThreadReceive();
	void ThreadSend();

	Controller* controller;
	unsigned long long listenSocket;
	unsigned long long hostSocket;
	bool isServer = false;
	bool connected = false;
	char messageToSend[512];
	int messageSize = 0;
	std::condition_variable sendCv;
	std::mutex mutex;
	bool sendShouldWake = false;
	bool lastDisconnectLocal = false;
	bool applicationClosed = false;
};

#endif // NETWORKERTCP_H