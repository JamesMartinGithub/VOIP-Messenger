#include "Controller.h"
#include "MainWindow.h"
#include "NetworkerTCP.h"
#include "NetworkerUDP.h"
#include "FileManager.h"
#include "AudioInput.h"
#include "AudioOutput.h"
#include <mmreg.h>
#include <iostream>

Controller::Controller() {
}

int Controller::Start(int argc, char* argv[]) {
	// Create UI window
	QApplication app(argc, argv);
	window = new MainWindow(this);
	window->show();

	// Initialise objects
	networkerTCP = new NetworkerTCP(this);
	networkerUDP = new NetworkerUDP(this);
	fileManager = new FileManager();
	audioInput = new AudioInput(this);
	audioOutput = new AudioOutput();

	// list input devices
	window->DisplayInputDevices(audioInput->GetDeviceNames());

	// Run UI logic
	int result = app.exec();
	// Application closed, cleanup
	applicationClosed = true;
	delete networkerTCP;
	delete networkerUDP;
	delete fileManager;
	delete audioInput;
	delete audioOutput;
	delete window;
	if (serverIp) {
		delete[] serverIp;
	}
	if (serverPort) {
		delete[] serverPort;
	}
	return result;
}

bool Controller::ToggleServer() {
	bool isNowServer = networkerTCP->ToggleServer();
	networkerUDP->SetServer(isNowServer);
	isServer = isNowServer;
	return isNowServer;
}

bool Controller::IsServer() {
	return isServer;
}

void Controller::Connect() {
	std::thread connectThread([&] { networkerTCP->Connect(nullptr, ""); });
	connectThread.detach();
}

void Controller::Connect(const char* ip, const char* port) {
	if (ip != "") {
		serverIp = new char[strlen(ip) + 1];
		strcpy(serverIp, ip);
	}
	if (port != "") {
		serverPort = new char[strlen(port) + 1];
		strcpy(serverPort, port);
	}
	std::thread connectThread([&] { networkerTCP->Connect(serverIp, serverPort); });
	connectThread.detach();
}

void Controller::Disconnect() {
	networkerTCP->Disconnect();
}

void Controller::DisplayConnectionState(std::string message, ConnectionType connection, bool isTCP) {
	if (isTCP && connection == ConnectionType::DISCONNECTED) {
		DisconnectCall(true);
		QMetaObject::invokeMethod(window, "DisplayConnectionState", Qt::QueuedConnection, Q_ARG(std::string, "Call ended"), Q_ARG(ConnectionType, ConnectionType::CONNECTING), Q_ARG(bool, false));
	}
	QMetaObject::invokeMethod(window, "DisplayConnectionState", Qt::QueuedConnection, Q_ARG(std::string, message), Q_ARG(ConnectionType, connection), Q_ARG(bool, isTCP));
}

bool Controller::SendData(const char* message, int size) {
	return networkerTCP->SendData(message, size);
}

void Controller::ReceiveData(char* message, int size) {
	switch (message[0]) {
		case '[': {
			// Received chat message
			std::string displayMessage;
			displayMessage.assign(message, size);
			QMetaObject::invokeMethod(window, "DisplayMessage", Qt::QueuedConnection, Q_ARG(std::string, displayMessage));
			break;
		}
		case '!': {
			// Received call control message
			switch (message[1]) {
				case 'C':
					// Friend is calling
					friendCalling = true;
					DisplayConnectionState("Incoming call...", ConnectionType::DISCONNECTED, false);
					break;
				case 'S':
					// Call connected, start sending data
					friendCalling = false;
					// Start audio processing
					if (!audioInput->Start()) {
						DisconnectCall();
					}
					DisplayConnectionState("Call connected", ConnectionType::CONNECTED, false);
					break;
				case 'D':
					// Friend ended call
					DisconnectCall(true);
					DisplayConnectionState("Friend ended call", ConnectionType::DISCONNECTED, false);
					break;
			}
			break;
		}
		case '?': {
			// Received audio format
			std::cout << "Received format, starting audio output\n";
			WAVEFORMATEX format;
			memcpy(&format, &message[1], sizeof(WAVEFORMATEX));
			// Start audio output
			if (!audioOutput->IsStarted()) {
				if (!audioOutput->Start(&format)) {
					DisconnectCall();
				}
			}
			break;
		}
		default:
			std::cout << "TCP Received invalid data";
			break;
	}
}

std::vector<std::string> Controller::GetFriendMessages(std::string name) {
	return fileManager->LoadFriendMessages(name);
}

void Controller::SaveMessage(std::string name, std::string message, bool isSent) {
	fileManager->StoreMessage(name, message, isSent);
}

std::vector<std::string> Controller::GetFriends() {
	return fileManager->LoadFriends();
}

void Controller::ModifyFriend(std::string name, bool isAdd) {
	if (isAdd) {
		fileManager->StoreFriend(name);
	}
	else {
		fileManager->RemoveFriend(name);
	}
}

void Controller::SetSourcePort(unsigned short portNum) {
	networkerUDP->SetSourcePort(portNum);
}

void Controller::SetDestinationAddress(sockaddr* address) {
	networkerUDP->SetDestinationAddress(address);
}

void Controller::ConnectCall() {
	bool isConnected = networkerUDP->Connect(serverIp);
	if (friendCalling) {
		// Notify friend to start sending audio data
		char* message = new char[3] {"!S"};
		SendData(message, 3);
		DisplayConnectionState("Call connected", ConnectionType::CONNECTED, false);
		// Start audio processing
		if (!audioInput->Start()) {
			DisconnectCall();
		}
	} else {
		// Notify friend host is calling
		char* message = new char[3] {"!C"};
		SendData(message, 3);
		DisplayConnectionState("Calling friend...", ConnectionType::DISCONNECTED, false);
	}
}

void Controller::DisconnectCall(bool disconnectByFriend) {
	friendCalling = false;
	audioInput->Stop();
	audioOutput->Stop();
	networkerUDP->Disconnect();
	if (!disconnectByFriend) {
		// Notify friend call ended, and to stop sending audio data
		char* message = new char[3] {"!D"};
		SendData(message, 3);
		DisplayConnectionState("Call ended", ConnectionType::DISCONNECTED, false);
	}
}

void Controller::SetInputDevice(int index) {
	audioInput->SetSelectedDevice(index);
}

void Controller::SendAudioFormat(WAVEFORMATEX* format) {
	char formatData[sizeof(WAVEFORMATEX) + 1];
	formatData[0] = '?';
	memcpy(&formatData[1], format, sizeof(WAVEFORMATEX));
	networkerTCP->SendData(formatData, sizeof(formatData));
}

void Controller::SendAudioData(unsigned char* buffer, unsigned int frameNum, int size) {
	std::cout << "AUDIO DATA SIZE: " << size << '\n';
	char* joinedData = new char[uintSize + size];
	memcpy(joinedData, &frameNum, uintSize);
	memcpy(&joinedData[uintSize], buffer, size);
	networkerUDP->SendData(joinedData, uintSize + size);
}

void Controller::ReceiveAudioData(char* data, int size) {
	if (!applicationClosed) {
		unsigned int frameNumUInt;
		unsigned char* buffer = new unsigned char[size - uintSize];
		memcpy(&frameNumUInt, data, uintSize);
		memcpy(buffer, &data[uintSize], size - uintSize);
		audioOutput->UpdateBuffer(buffer, frameNumUInt);
	}
}
