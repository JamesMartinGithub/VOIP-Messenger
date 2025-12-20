#include "NetworkerUDP.h"
#include "Controller.h"
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")
#define DEFAULT_BUFLEN 8192

NetworkerUDP::NetworkerUDP(Controller* pController)
	: controller(pController)
	, sendDataBuffer(35) {
}

NetworkerUDP::~NetworkerUDP() {
	applicationClosed = true;
	Disconnect();
	WSACleanup();
}

void NetworkerUDP::SetServer(bool pIsServer) {
	isServer = pIsServer;
}

void NetworkerUDP::SetSourcePort(unsigned short portNum) {
	sourcePort = ntohs(portNum);
}

void NetworkerUDP::SetDestinationAddress(sockaddr* address) {
	dest = *reinterpret_cast<sockaddr_in*>(address);
	destSize = sizeof(dest);
}

bool NetworkerUDP::IsConnected() {
	return connected;
}

bool NetworkerUDP::Connect(const char* ip) {
	if (!connected) {
		sockaddr_in local;
		WSAData data;

		// Initialize Winsock DLL
		int resultCode = WSAStartup(MAKEWORD(2, 2), &data);
		if (resultCode == SOCKET_ERROR) {
			printf("UDP startup error: %d\n", WSAGetLastError());
			CancelInitialise("Unable to connect voice");
			return false;
		}

		// Define desired local address type
		struct addrinfo* result = NULL, * ptr = NULL, hints;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		hints.ai_flags = AI_PASSIVE;

		// Resolve the local address and port
		char sourcePortChars[6];
		sprintf(sourcePortChars, "%i", sourcePort);
		resultCode = getaddrinfo("", sourcePortChars, &hints, &result);
		if (resultCode != 0) {
			printf("UDP src getaddrinfo failed: %d\n", resultCode);
			CancelInitialise("Unable to connect voice");
			return false;
		}
		local = *reinterpret_cast<sockaddr_in*>(result->ai_addr);

		// Create socket
		hostSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (hostSocket == INVALID_SOCKET) {
			printf("UDP socket error: %d\n", WSAGetLastError());
			CancelInitialise("Unable to connect voice");
			return false;
		}
		// Enable socket to re-use TCP port
		BOOL reuseaddrOptVal = TRUE;
		int bOptLen = sizeof(BOOL);
		setsockopt(hostSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddrOptVal, bOptLen);
		// Disable socket lingering when closesocket() called
		BOOL dontlingerOptVal = TRUE;
		setsockopt(hostSocket, SOL_SOCKET, SO_DONTLINGER, (char*)&dontlingerOptVal, bOptLen);
		// Set send timeout to 1 second
		struct timeval sendtimeoutOptVal;
		sendtimeoutOptVal.tv_sec = 1;
		sendtimeoutOptVal.tv_usec = 0;
		setsockopt(hostSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&sendtimeoutOptVal, sizeof(timeval));
		 

		// Bind local ip to socket
		resultCode = bind(hostSocket, (sockaddr*)&local, sizeof(local));
		if (resultCode == SOCKET_ERROR) {
			printf("UDP bind error: %d\n", WSAGetLastError());
			CancelInitialise("Unable to connect voice");
			return false;
		}

		destSize = sizeof(dest);
		connected = true;
		WSASetLastError(0);
		
		// Start sending and receiving threads
		std::thread receiveThread([this] { this->ThreadReceive(); });
		std::thread sendThread([this] { this->ThreadSend(); });
		receiveThread.detach();
		sendThread.detach();
		printf("UDP started!\n");

		// Print local and destination sockets for debugging
		sockaddr thisAddress;
		int thisAddressSize = sizeof(sockaddr);
		getsockname(hostSocket, &thisAddress, &thisAddressSize);
		printf("UDP this host port: %i\n", ntohs(((sockaddr_in*)&thisAddress)->sin_port));
		printf("UDP other host port: %i\n", ntohs(dest.sin_port));
		return true;
	} else {
		return false;
	}
}

void NetworkerUDP::Disconnect() {
	shutdown(hostSocket, SD_BOTH);
	closesocket(hostSocket);
	CancelInitialise("");
}

void NetworkerUDP::CancelInitialise(std::string disconnectMessage) {
	connected = false;
	sendCv.notify_one();
	Data* data;
	while (sendDataBuffer.try_pop(data)) {
		delete data;
	}
	if (disconnectMessage != "" && !applicationClosed) {
		controller->DisplayConnectionState(disconnectMessage, Controller::ConnectionType::DISCONNECTED, false);
	}
}

void NetworkerUDP::ThreadReceive() {
	char recvbuf[DEFAULT_BUFLEN];
	int resultCode;
	int recvbuflen = DEFAULT_BUFLEN;
	do {
		// Block thread until data received
		resultCode = recvfrom(hostSocket, recvbuf, recvbuflen, 0, (sockaddr*)&dest, &destSize);
		if (resultCode > 0) {
			// Successfully received data
			controller->ReceiveAudioData(recvbuf, resultCode);
		} else {
			// Socket error occurred
			int lastError = WSAGetLastError();
			printf("UDP recv failed: %d\n", lastError);
			if (lastError == WSAECONNRESET) {
				controller->DisconnectCall(true);
			} else {
				controller->DisconnectCall();
				break;
			}
		}
	} while (resultCode > 0);
}

void NetworkerUDP::ThreadSend() {
	int resultCode = 0;
	do {
		// Block thread until notified by sendShouldWake
		std::unique_lock lock(mutex);
		sendCv.wait(lock, [&] { return !sendDataBuffer.empty() || !connected; });
		lock.unlock();
		while (!sendDataBuffer.empty() && connected) {
			// Attempt to pop data from queue
			Data* data;
			if (sendDataBuffer.try_pop(data)) {
				// Data found, send over network
				resultCode = sendto(hostSocket, data->data, data->size, 0, (sockaddr*)&dest, destSize);
				// Check for socket error
				if (resultCode == SOCKET_ERROR) {
					int lastError = WSAGetLastError();
					if (lastError == WSAETIMEDOUT) {
						// Send timed out, free unsent data memory
						delete data;
					} else {
						printf("UDP sendto failed: %d\n", lastError);
						controller->DisconnectCall();
						break;
					}
				} else {
					// Send was successful, free data memory
					delete data;
				}
			}
		}
	} while (connected);
}

void NetworkerUDP::SendData(char* message, int size) {
	if (connected) {
		Data* data = new Data(message, size);
		if (!sendDataBuffer.try_emplace(data)) {
			delete data;
		}
	} else {
		delete[] message;
	}
}

void NetworkerUDP::NotifySendThread() {
	sendCv.notify_one();
}