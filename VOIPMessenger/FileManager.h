#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <vector>
#include <string>

class Controller;

class FileManager {
public:
	FileManager();
	std::vector<std::string> LoadFriendMessages(std::string name);
	void StoreMessage(std::string name, std::string message, bool isSent);
	std::vector<std::string> LoadFriends();
	void StoreFriend(std::string name);
	void RemoveFriend(std::string name);
};

#endif // FILEMANAGER_H