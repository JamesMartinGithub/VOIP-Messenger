#include "NetworkerUDP.h"
#include "Controller.h"
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")
#define DEFAULT_BUFLEN 8192

NetworkerUDP::NetworkerUDP(Controller* pController)
	: controller(pController) {
}

NetworkerUDP::~NetworkerUDP() {
	applicationClosed = true;
	shutdown(hostSocket, SD_BOTH);
	closesocket(hostSocket);
	WSACleanup();
	{
		std::lock_guard lk(mutex);
		sendShouldWake = true;
	}
	sendCv.notify_one();
	while (bufferCount > 0) {
		delete[] dataBuffer.front();
		dataBuffer.pop();
		bufferCount--;
	}
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

bool NetworkerUDP::Connect(const char* ip) {
	if (!connected) {
		lastDisconnectLocal = false;
		sockaddr_in local;
		WSAData data;

		// Initialize Winsock DLL
		int resultCode = WSAStartup(MAKEWORD(2, 2), &data);
		if (resultCode == SOCKET_ERROR) {
			std::cout << "UDP STARTUP ERROR: " << WSAGetLastError() << "\n";
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
		resultCode = hostSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (resultCode == SOCKET_ERROR) {
			std::cout << "UDP socket error: " << WSAGetLastError() << "\n";
			CancelInitialise("Unable to connect voice");
			return false;
		}
		// Enable socket to re-use TCP port
		BOOL bOptVal = TRUE;
		int bOptLen = sizeof(BOOL);
		setsockopt(hostSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&bOptVal, bOptLen);

		// Bind local ip to socket
		resultCode = bind(hostSocket, (sockaddr*)&local, sizeof(local));
		if (resultCode == SOCKET_ERROR) {
			std::cout << "UDP bind error: " << WSAGetLastError() << "\n";
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
		std::cout << "UDP started!" << '\n';

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
	lastDisconnectLocal = true;
	shutdown(hostSocket, SD_BOTH);
	closesocket(hostSocket);
	CancelInitialise("");
}

void NetworkerUDP::CancelInitialise(std::string disconnectMessage) {
	connected = false;
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
			int lastError = WSAGetLastError();
			if (lastError == WSAECONNRESET) {
				std::cout << "ICMP unreachable server" << '\n';
			} else {
				// Connection error
				printf("UDP recv failed: %d\n", lastError);
				shutdown(hostSocket, SD_BOTH);
				closesocket(hostSocket);
				CancelInitialise(lastDisconnectLocal ? "" : "Friend disconnected");
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
		sendCv.wait(lock, [this] { return sendShouldWake; });
		sendShouldWake = false;
		if (applicationClosed) {
			break;
		}
		// Send message over network
		if (bufferCount > 0) {
			resultCode = sendto(hostSocket, dataBuffer.front(), dataSizeBuffer.front(), 0, (sockaddr*)&dest, destSize);
			delete[] dataBuffer.front();
			dataBuffer.pop();
			dataSizeBuffer.pop();
			bufferCount--;
		} else {
			resultCode = 0;
		}
		if (resultCode == SOCKET_ERROR) {
			if (applicationClosed) {
				break;
			} else {
				int lastError = WSAGetLastError();
				if (lastError != 0) {
					printf("UDP sendto failed: %d\n", lastError);
					controller->DisconnectCall();
					break;
				}
			}
		}
		messageSize = 0;
	} while (true);
}

bool NetworkerUDP::SendData(char* message, int size) {
	if (connected) {
		dataBuffer.push(message);
		dataSizeBuffer.push(size);
		bufferCount++;
		sendShouldWake = true;
		{
			std::lock_guard lk(mutex);
			sendShouldWake = true;
		}
		sendCv.notify_one();
		return true;
	} else {
		return false;
	}
}