#pragma once
#include <string>
#include <chrono>
#include <unordered_map>
#include <vector>
class Server{
private:
	struct ReplicationInfo{
	std::string role;
	size_t connected_slaves;
	std::string master_replid;
	int master_repl_offset;
	int second_repl_offset;
	int repl_backlog_active;
	int repl_backlog_size;
	int repl_backlog_first_byte_offset;
	int repl_backlog_histlen;
	};
	struct SlaveInfo {
	std::string ip;
	int port;
	};
	enum class Handshake_Stage {
	PING,
	REPLCONF1,
	REPLCONF2,
	PSYNC,
	};
public:
	ReplicationInfo replInfo;
	Server() : port(6379){};//default
	void generateVars(int argc, char* argv[]);
	void run();
	void setKey(std::string key, std::string value, std::string ttl);
	std::string getKeyValue(std::string key);
	std::string getRole() const { return replInfo.role; }
	std::string getMasterHost() const { return masterHost; }
	int getMasterPort() const { return masterPort; }
	std::vector<SlaveInfo> connectedSlaves;
	std::unordered_map<int, Handshake_Stage> replicaHandshakeMap;
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
	std::string masterHost;
	int masterPort;
	RdbSize getSize(std::string* fileContents, size_t* cursor);
	void handleDbRead(std::string* fileContents, size_t* cursor);
	void loadRDBfile(std::string dir, std::string dbfilename);
	void sendHandshake();
	std::vector<std::string> generateCommands(const char charBuffer[1024]);
	std::string handleIndividualWord(const char charBuffer[1024], int* wordIndex);
	std::string fetchKeys(std::string key);
	void sendData(const std::vector<std::string> &commands, int client_socket);
	
};

