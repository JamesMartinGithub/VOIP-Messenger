#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QApplication>
#include <string>
#include <list>
#include <winsock2.h>

class MainWindow;
class NetworkerTCP;
class NetworkerUDP;
class FileManager;
class AudioInput;
class AudioOutput;
struct tWAVEFORMATEX;
typedef tWAVEFORMATEX WAVEFORMATEX;

class Controller {
public:
	enum class ConnectionType : unsigned int {
		DISCONNECTED,
		CONNECTING,
		CONNECTED
	};

	Controller();
	int Start(int argc, char* argv[]);
	// Text chat network methods
	bool ToggleServer();
	bool IsServer();
	void Connect();
	void Connect(const char* ip, const char* port);
	void Disconnect();
	void DisplayConnectionState(std::string message, ConnectionType connection, bool isTCP);
	bool SendData(const char* message, int size);
	void ReceiveData(char* message, int size);
	// Storage methods
	std::vector<std::string> GetFriendMessages(std::string name);
	void SaveMessage(std::string name, std::string message, bool isSent);
	std::vector<std::string> GetFriends();
	void ModifyFriend(std::string name, bool isAdd);
	// Voice chat network methods
	void SetSourcePort(unsigned short portNum);
	void SetDestinationAddress(sockaddr* address);
	void ConnectCall();
	void DisconnectCall(bool disconnectByFriend = false);
	void SetInputDevice(int index);
	void SendAudioFormat(WAVEFORMATEX* format);
	void SendAudioData(unsigned char* buffer, unsigned int frameNum, int size);
	void ReceiveAudioData(char* data, int size);

private:
	MainWindow* window;
	NetworkerTCP* networkerTCP;
	NetworkerUDP* networkerUDP;
	FileManager* fileManager;
	AudioInput* audioInput;
	AudioOutput* audioOutput;
	bool friendCalling = false;
	char* serverIp = nullptr;
	char* serverPort = nullptr;
	const int uintSize = sizeof(unsigned int);
	bool isServer = false;
	bool applicationClosed = false;
};

#endif // CONTROLLER_H
