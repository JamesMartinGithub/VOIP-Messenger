#include "NetworkerTCP.h"
#include "Controller.h"
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")
#define LISTEN_PORT (unsigned short)34153
#define DEFAULT_BUFLEN 512

NetworkerTCP::NetworkerTCP(Controller* pController)
	: controller(pController) {
}

NetworkerTCP::~NetworkerTCP() {
	applicationClosed = true;
	closesocket(listenSocket);
	shutdown(hostSocket, SD_BOTH);
	closesocket(hostSocket);
	WSACleanup();
	{
		std::lock_guard lk(mutex);
		sendShouldWake = true;
	}
	sendCv.notify_one();
}

bool NetworkerTCP::ToggleServer() {
	if (!connected) {
		isServer = !isServer;
		return isServer;
	}
	else {
		if (isServer) {
			return true;
		}
		else {
			return false;
		}
	}
}

bool NetworkerTCP::Connect(const char* ip, const char* port) {
	if (!connected) {
		lastDisconnectLocal = false;
		// Initialize Winsock DLL
		WSADATA wsaData;
		int resultCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (resultCode != 0) {
			printf("WSAStartup failed: %d\n", resultCode);
			CancelInitialise("Network error");
			return false;
		}

		// Create either server or client connection
		if (isServer) {
			// Application is server
			// Define desired connection type
			struct addrinfo* result = NULL, * ptr = NULL, hints;
			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;
			hints.ai_flags = AI_PASSIVE;

			// Resolve the local address and port to be used by the server
			resultCode = getaddrinfo("", NULL, &hints, &result);
			if (resultCode != 0) {
				printf("getaddrinfo failed: %d\n", resultCode);
				CancelInitialise("Unable to open connection");
				return false;
			}

			// Create socket to listen for client
			listenSocket = INVALID_SOCKET;
			listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
			if (listenSocket == INVALID_SOCKET) {
				printf("Error at socket(): %ld\n", WSAGetLastError());
				freeaddrinfo(result);
				CancelInitialise("Unable to open connection");
				return false;
			}

			// Bind listen socket to address
			((sockaddr_in*)(result->ai_addr))->sin_port = htons(LISTEN_PORT);
			resultCode = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
			if (resultCode == SOCKET_ERROR) {
				printf("Bind failed with error: %d\n", WSAGetLastError());
				freeaddrinfo(result);
				closesocket(listenSocket);
				CancelInitialise("Unable to open connection");
				return false;
			}

			// Free uneeded result
			freeaddrinfo(result);

			// Start listening for client connection request
			if (listen(listenSocket, 1) == SOCKET_ERROR) {
				printf("Listen failed with error: %ld\n", WSAGetLastError());
				closesocket(listenSocket);
				CancelInitialise("Unable to open connection");
				return false;
			}
			std::cout << "Server listening for client connection..." << '\n';
			char connectingMessage[45];
			sprintf(connectingMessage, "Connection open, listening on port: %i...", ntohs(((sockaddr_in*)result->ai_addr)->sin_port));
			controller->DisplayConnectionState(std::string(connectingMessage), Controller::ConnectionType::CONNECTING, true);

			// Accept client connection request
			hostSocket = INVALID_SOCKET;
			struct sockaddr clientAddress = { 0 };
			socklen_t clientAddressLength = sizeof(clientAddress);
			hostSocket = accept(listenSocket, &clientAddress, &clientAddressLength);
			if (hostSocket == INVALID_SOCKET) {
				printf("accept failed: %d\n", WSAGetLastError());
				closesocket(listenSocket);
				CancelInitialise(lastDisconnectLocal ? "" : "Unable to connect");
				return false;
			}

			// Free uneeded listen socket
			closesocket(listenSocket);

			connected = true;
			std::cout << "Accepted client connection!" << '\n';

			// Start sending and receiving threads
			std::thread receiveThread([this] { this->ThreadReceive(); });
			std::thread sendThread([this] { this->ThreadSend(); });
			receiveThread.detach();
			sendThread.detach();
		}
		else {
			// Application is client
			// Define desired connection type
			struct addrinfo* result = NULL, * ptr = NULL, hints;
			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;

			// Resolve the server address and port
			resultCode = getaddrinfo(ip, port, &hints, &result);
			if (resultCode != 0) {
				printf("getaddrinfo failed: %d\n", resultCode);
				CancelInitialise("Unable to connect");
				return false;
			}

			// Create socket to connect to server
			hostSocket = INVALID_SOCKET;
			ptr = result;
			hostSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
			if (hostSocket == INVALID_SOCKET) {
				printf("Error at socket(): %ld\n", WSAGetLastError());
				freeaddrinfo(result);
				CancelInitialise("Unable to connect");
				return false;
			}

			// Connect to server.
			resultCode = connect(hostSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (resultCode == SOCKET_ERROR) {
				closesocket(hostSocket);
				hostSocket = INVALID_SOCKET;
			}

			// Free uneeded result
			freeaddrinfo(result);
			if (hostSocket == INVALID_SOCKET) {
				printf("Unable to connect to server\n");
				CancelInitialise("Unable to connect");
				return false;
			}

			connected = true;
			std::cout << "Connected to server!" << '\n';

			// Start sending and receiving threads
			std::thread receiveThread([this] { this->ThreadReceive(); });
			std::thread sendThread([this] { this->ThreadSend(); });
			receiveThread.detach();
			sendThread.detach();
		}
		// Print local and destination sockets for debugging
		sockaddr thisAddress;
		int thisAddressSize = sizeof(sockaddr);
		getsockname(hostSocket, &thisAddress, &thisAddressSize);
		printf("TCP this host port: %i\n", ntohs(((sockaddr_in*)&thisAddress)->sin_port));
		sockaddr otherAddress;
		int otherAddressSize = sizeof(sockaddr);
		getpeername(hostSocket, &otherAddress, &otherAddressSize);
		printf("TCP other host port: %i\n", ntohs(((sockaddr_in*)&otherAddress)->sin_port));

		// Send local port and destination address to controller for UDP connection
		controller->SetSourcePort(((sockaddr_in*)&thisAddress)->sin_port);
		controller->SetDestinationAddress(&otherAddress);

		controller->DisplayConnectionState("Connected", Controller::ConnectionType::CONNECTED, true);
		return true;
	}
	else {
		return false;
	}
}

void NetworkerTCP::Disconnect() {
	lastDisconnectLocal = true;
	shutdown(hostSocket, SD_BOTH);
	closesocket(hostSocket);
	CancelInitialise("Disconnected");
}

void NetworkerTCP::CancelInitialise(std::string disconnectMessage) {
	WSACleanup();
	connected = false;
	if (disconnectMessage != "" && !applicationClosed) {
		controller->DisplayConnectionState(disconnectMessage, Controller::ConnectionType::DISCONNECTED, true);
	}
}

void NetworkerTCP::ThreadReceive() {
	char recvbuf[DEFAULT_BUFLEN];
	int resultCode;
	int recvbuflen = DEFAULT_BUFLEN;
	do {
		// Block thread until data received
		resultCode = recv(hostSocket, recvbuf, recvbuflen, 0);
		if (resultCode > 0) {
			// Successfully received message
			printf("TCP received message: %.*s\n", 2, recvbuf);
			controller->ReceiveData(recvbuf, resultCode);
		}
		else {
			// Connection closed or error
			if (resultCode == 0) {
				if (isServer) {
					printf("Connection closed by client\n");
				}
				else {
					printf("Connection closed by server\n");
				}
			}
			else {
				printf("recv failed: %d\n", WSAGetLastError());
			}
			shutdown(hostSocket, SD_BOTH);
			closesocket(hostSocket);
			CancelInitialise(lastDisconnectLocal ? "" : "Friend disconnected");
			break;
		} 
	} while (resultCode > 0);
}

void NetworkerTCP::ThreadSend() {
	int resultCode;
	do {
		// Block thread until notified by sendShouldWake
		std::unique_lock lock(mutex);
		sendCv.wait(lock, [this] { return sendShouldWake; });
		sendShouldWake = false;
		// Send message over network
		resultCode = send(hostSocket, messageToSend, messageSize, 0);
		if (resultCode == SOCKET_ERROR) {
			printf("Send failed: %d\n", WSAGetLastError());
			closesocket(hostSocket);
			WSACleanup();
			break;
		}
		printf("TCP sent message: %.*s\n", 2, messageToSend);
		messageSize = 0;
	} while (true);
}

bool NetworkerTCP::SendData(const char* message, int size) {
	if (connected) {
		memcpy(messageToSend, message, size);
		messageSize = size;
		sendShouldWake = true;
		{
			std::lock_guard lk(mutex);
			sendShouldWake = true;
		}
		sendCv.notify_one();
		return true;
	}
	else {
		return false;
	}
}
