#include "FileManager.h"
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace std;

#define FILE_NAME "messages.txt"

FileManager::FileManager() {
}

vector<string> FileManager::LoadFriendMessages(string name) {
	vector<string> messages;
	// First check if the messages file exists
	if (filesystem::exists(FILE_NAME)) {
		int nameLength = name.length();
		ifstream file(FILE_NAME, ifstream::in);
		string line;
		// Iterate through file lines
		while (getline(file, line)) {
			// Check if line name matches friend name
			if (line.substr(0, nameLength) == name) {
				int msgStart = nameLength + 1;
				int count = 0;
				// iterate over line to add comma-seperated messages to vector
				for (int i = nameLength + 1; i < line.length() - 2; i++) {
					if (line[i] == ',') {
						string saveMessage = line.substr(msgStart, count);
						// Replace all instances of comma with special char
						replace(saveMessage.begin(), saveMessage.end(), '\x91', ',');
						messages.push_back(saveMessage);
						msgStart = i + 1;
						count = 0;
					}
					else {
						count++;
					}
				}
				string saveMessage = line.substr(msgStart, count + 1);
				if (saveMessage != "") {
					// Replace all instances of comma with special char
					replace(saveMessage.begin(), saveMessage.end(), '\x91', ',');
					// Reached end of line, add last message
					messages.push_back(saveMessage);
				}
				break;
			}
		}
		file.close();
		return messages;
	}
	else { 
		return messages;
	}
}

void FileManager::StoreMessage(std::string name, std::string message, bool isSent) {
	int nameLength = name.length();
	fstream file(FILE_NAME, fstream::in);
	string line;
	vector<string> allLines;
	// Iterate through file lines
	while (getline(file, line)) {
		// Check if line name matches friend name
		if (line.substr(0, nameLength) == name) {
			// Replace all instances of comma with special char
			replace(message.begin(), message.end(), ',', '\x91');
			// Append message to friend message list
			allLines.push_back(line + (isSent ? 'S' : 'R') + message + ',');
		}
		else {
			allLines.push_back(line);
		}
	}
	// Clear file
	file.close();
	file.open(FILE_NAME, fstream::out | fstream::trunc);
	// Re-add all lines
	for (string s : allLines) {
		file << s << '\n';
	}
	file.close();
}

std::vector<std::string> FileManager::LoadFriends() {
	vector<string> friends;
	// First check if the messages file exists
	if (filesystem::exists(FILE_NAME)) {
		ifstream file(FILE_NAME, ifstream::in);
		string line;
		// Iterate through file lines
		while (getline(file, line)) {
			friends.push_back(line.substr(0, line.find(':')));
		}
		file.close();
		return friends;
	}
	else {
		return friends;
	}
}

void FileManager::StoreFriend(std::string name) {
	ofstream file(FILE_NAME, ofstream::out | ofstream::app);
	// Append name to file
	file << name << ":\n";
	file.close();
}

void FileManager::RemoveFriend(std::string name) {
	int nameLength = name.length();
	fstream file(FILE_NAME, fstream::in);
	string line;
	vector<string> allLines;
	// Iterate through file lines
	while (getline(file, line)) {
		// Check if line name matches friend name, exclude line from vector if so
		if (!(line.substr(0, nameLength) == name)) {
			allLines.push_back(line);
		}
	}
	// Clear file
	file.close();
	file.open(FILE_NAME, fstream::out | fstream::trunc);
	// Re-add all lines
	for (string s : allLines) {
		file << s << '\n';
	}
	file.close();
}
