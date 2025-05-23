#pragma once
#include <string>
#include <chrono>
#include <unordered_map>
#include <vector>
class Server{
public:
	Server() : port(6379){};//default
	void generateVars(int argc, char* argv[]);
	void run();
	void setKey(std::string key, std::string value, std::string ttl);
	std::string getKeyValue(std::string key);

private:
	struct RdbSize{
		bool special_encoding;
		size_t size;
	};
	struct keyInfo{
		std::string value;
		std::chrono::system_clock::time_point timestamp;
		std::chrono::milliseconds ttl;
	};
	std::unordered_map<std::string, keyInfo> dataStore;
	std::string dir;
	std::string dbfilename;
	int port;
	RdbSize getSize(std::string* fileContents, size_t* cursor);
	void handleDbRead(std::string* fileContents, size_t* cursor);
	void loadRDBfile(std::string dir, std::string dbfilename);
	std::vector<std::string> generateCommands(const char charBuffer[1024]);
	std::string handleIndividualWord(const char charBuffer[1024], int* wordIndex);
	std::string fetchKeys(std::string key);
	void sendData(const std::vector<std::string> &commands, int client_socket);
	
};

