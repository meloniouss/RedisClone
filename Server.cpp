#include <iostream>
#include <ws2tcpip.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <winsock2.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <chrono>
#include <regex>
#include "server.hpp"

void Server::setKey(std::string key, std::string value, std::string ttl)
{
  keyInfo key_Value{
  value,
  std::chrono::system_clock::now(),
  std::chrono::milliseconds(std::stoi(ttl))
  };
  dataStore.emplace(key, key_Value);
}

std::string Server::getKeyValue(std::string key){
  auto it = dataStore.find(key);
  if(it != dataStore.end()){
    auto expiryTime = (it->second.timestamp + it->second.ttl);
    auto currentTime = std::chrono::system_clock::now();
    if(it->second.ttl == std::chrono::milliseconds(-1) || currentTime < expiryTime){ //if ttl = -1 -> not meant to expire at all
      return it->second.value;
    }
  }
  dataStore.erase(key);
  return ""; //check for empty string in the command function
}

Server::RdbSize Server::getSize(std::string* fileContents, size_t* cursor){  
 RdbSize returnSize;
  std::cout << "Cursor: " << *cursor << ", file size: " << fileContents->size() << "\n";
  if (*cursor >= fileContents->size()) {
        std::cerr << "Error: Cursor position out of bounds when reading size\n";
        return returnSize;
    }
  uint8_t firstByte = static_cast<uint8_t>((*fileContents)[*cursor]);
  uint8_t twoFirstBits = firstByte >> 6;
  size_t size=0;
    
  if(twoFirstBits == 0b00){ //last 6 bits
    size = (firstByte & 0x3F);
    (*cursor)++;
  }
  else if(twoFirstBits == 0b01){ //last 6 bits + next byte
    (*cursor)++; //get next byte
    size = (firstByte & 0x3F) + static_cast<uint8_t>((*fileContents)[(*cursor)++]); 
  }
  else if(twoFirstBits == 0b10){
    (*cursor)++; // go to next byte
    if(firstByte == 0x80){ //read next 4 bytes
      size = 0;
      for (size_t i = 0; i < 4; ++i){
        size |= (static_cast<uint64_t>(static_cast<uint8_t>((*fileContents)[(*cursor)++])) << (8 * i));
      }   
      (*cursor)++; // new
    }
    else if(firstByte == 0x81){ //read next 8 bytes
      size = 0;
      for (size_t i = 0; i < 8; ++i){   
        size |= (static_cast<uint64_t>(static_cast<uint8_t>((*fileContents)[(*cursor)++])) << (8 * i));
      }
      (*cursor)++; //new
    }
  }
  else if(twoFirstBits == 0b11){ //specific string encoding
    returnSize.special_encoding = true;
    size = (firstByte);
    (*cursor)++;
  }
  returnSize.size = size;
  return returnSize;
}
void Server::handleDbRead(std::string* fileContents, size_t* cursor){
  (*cursor)++;
  auto dbIndex = getSize(fileContents, cursor).size;
  std::cout << "dbIndex: " << dbIndex << std::endl;
  size_t numKeys = 0;
  size_t numExpKeys = 0;
  while(*cursor < fileContents->size() && static_cast<unsigned char>((*fileContents)[*cursor]) != 0xFB){(*cursor)++;}
  (*cursor)++; //skip 0xFB byte
  if (*cursor >= fileContents->size()) {
    std::cerr << "Error: 0xFB byte not found, cursor out of bounds\n";
    return; 
}
  RdbSize keySize = getSize(fileContents, cursor);
  numKeys = keySize.size;
  RdbSize expKeySize = getSize(fileContents, cursor);
  numExpKeys = expKeySize.size;
  std::cout << "Num keys: " << numKeys << ", Expiring: " << numExpKeys << "\n";
  size_t i = 0;
  auto ttl = std::chrono::milliseconds(0);
  std::stringstream keyStrStream;
  std::stringstream valueStrStream;
  size_t keyLength = 0;
  size_t valueLength = 0;
  std::cin.get(); //remove after testing  
  (*cursor)++;
  while(i < numKeys && (*cursor) <= fileContents->size()){
    if(!valueStrStream.str().empty()){
      std::cout << "ttl is: " << std::to_string(ttl.count());
      ttl = (ttl == std::chrono::milliseconds(0)) ? std::chrono::milliseconds(-1): ttl;
      setKey(keyStrStream.str(), valueStrStream.str(), std::to_string(ttl.count()));
      ttl = std::chrono::milliseconds(0);
      keyStrStream.str("");
      keyStrStream.clear();
      valueStrStream.str("");
      valueStrStream.clear();
      keyLength = 0;
      valueLength = 0;
      i++;
      std::cout << "NUM KEYS PROCESSED IS: " << i << std::endl;
      (*cursor)++; // new
    }                                    
    else if(static_cast<unsigned char>((*fileContents)[(*cursor)]) == 0xFC){ //milliseconds, careful with the incrementing here
      std::cout << "FOUND KEY WITH EXPIRATION" << std::endl;
      std::cin.get();
      uint64_t expirationRaw = 0;
      for(size_t j = 0; j < 8; j++){
        expirationRaw |= static_cast<uint64_t>(static_cast<unsigned char>((*fileContents)[(*cursor)])) << (8 * j);
        (*cursor)++;
      }
      auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
      ttl = std::chrono::milliseconds(expirationRaw) - now_ms;
    }                                                 //added ++ here
    else if(static_cast<unsigned char>((*fileContents)[(*cursor)]) == 0xFD){ //seconds transformed into milliseconds
      std::cout << "FOUND 0xFD expiration" << std::endl;
      std::cin.get();
      uint32_t expireSeconds = 0;
      for(size_t j = 0; j < 4; j++) {
        expireSeconds |= (static_cast<uint32_t>(static_cast<unsigned char>((*fileContents)[*cursor])) << (8 * j));
        (*cursor)++;
      }
      auto expireMillis = std::chrono::milliseconds(expireSeconds * 1000ULL);
      ttl = expireMillis - std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    
    }
    else{
      if(keyStrStream.str().empty()){ //read key
        std::cout << "Reading key size at cursor: " << *cursor << std::endl;
        std::cin.get(); //remove
        RdbSize keySize = getSize(fileContents,cursor);
        keyLength = keySize.size;
        std::cout << "Key length is:" << keyLength << std::endl;
        std::cin.get(); //remove
        for(size_t index = 0; index < keyLength; index++){
          keyStrStream <<static_cast<unsigned char>((*fileContents)[*cursor]);
          (*cursor)++;
        }
      }
      if(valueStrStream.str().empty()){ //read value
        RdbSize valueLength = getSize(fileContents,cursor);
        bool isSpecial = valueLength.special_encoding; 
        if((isSpecial)) //special string encoding
        {
          size_t byte = valueLength.size;
          switch (byte){
            case 0xC0: {//8bit int, endian-ness doesn't apply
              uint8_t value = static_cast<uint8_t>((*fileContents)[(*cursor)++]);
              valueStrStream << std::to_string(value);
              break;
            }
            case 0xC1: {//16 bit int, little endian
              uint16_t val = 0;
              val |= static_cast<uint16_t>(static_cast<uint8_t>((*fileContents)[(*cursor)++]));
              val |= static_cast<uint16_t>(static_cast<uint8_t>((*fileContents)[(*cursor)++])) << 8;
              valueStrStream << std::to_string(val);
              break;
            }
            case 0xC2:{ //32 bit int, little endian
              uint32_t val = 0;
              val |= static_cast<uint32_t>(static_cast<uint8_t>((*fileContents)[(*cursor)++]));
              val |= static_cast<uint32_t>(static_cast<uint8_t>((*fileContents)[(*cursor)++])) << 8;
              val |= static_cast<uint32_t>(static_cast<uint8_t>((*fileContents)[(*cursor)++])) << 16;
              val |= static_cast<uint32_t>(static_cast<uint8_t>((*fileContents)[(*cursor)++])) << 24;
              valueStrStream << std::to_string(val);
            break;
            }
            case 0xC3:
              //ignore, not doing compression, maybe add some error handling or soemthing
            break;
          }
        }
        else{ //regular encoded
          for(size_t index = 0; index < valueLength.size; index++){
            valueStrStream <<static_cast<unsigned char>((*fileContents)[(*cursor)++]);
          }
        }
      }
    }
  }
  if (!valueStrStream.str().empty()) {
    ttl = (ttl == std::chrono::milliseconds(0)) ? std::chrono::milliseconds(-1): ttl;
    setKey(keyStrStream.str(), valueStrStream.str(), std::to_string(ttl.count()));
  }
}
void Server::loadRDBfile(std::string dir, std::string dbfilename){
  //all numbers are little endian (stored in reverse order)
  std::cout << "loading rdb file" << std::endl;
  std::filesystem::path filepath = std::filesystem::path(dir) / dbfilename; 
  std::cout << filepath << std::endl;
  std::ifstream RDB_FILE(filepath, std::ios::binary);
  if(!RDB_FILE){
    std::cerr << "ERROR: Invalid RDB file, make sure to use an absolute path." << std::endl;
  }
  auto fileSize = std::filesystem::file_size(filepath);
  std::string fileContents(fileSize, '\0');
  if(!RDB_FILE.read(fileContents.data(), fileSize)){
    std::cerr << "ERROR: Failed to read RDB file.\n";
  }
  size_t cursor = 0;
std::cout << "First 30 bytes:\n";
for (size_t i = 0; i < 30 && i < fileContents.size(); i++) {
    printf("%02X ", static_cast<unsigned char>(fileContents[i]));
}
std::cout << "\n";
  while(cursor < fileContents.size()){
    if(cursor + 6 <= fileContents.size() && fileContents.substr(cursor, 6) == "REDIS"){
      cursor += 10;
    }
    else {
      unsigned char currByte = static_cast<unsigned char>(fileContents[cursor]);
      if(currByte == 0xFE){
          std::cout << "reading DB function called";
          handleDbRead(&fileContents, &cursor);
      }
      else if(currByte == 0xFF){
          break;
      }
      else{
          cursor++;
      }
    }
  }
}

void Server::generateVars(int argc, char* argv[]){
  for (size_t i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--dir" && i + 1 < argc) {
      dir = argv[++i];
    } 
    else if (arg == "--dbfilename" && i + 1 < argc) {
      dbfilename = argv[++i];
    }
    else if(arg == "--port" && i+1 < argc){
        port = std::stoi(argv[++i]);
    }
    else if(arg == "--replicaof" && i+2 < argc){
      replInfo.role = "slave";
      masterHost = argv[++i];
      masterPort = std::stoi(argv[++i]);
    }
    else{
      std::cerr << "Unknown or incomplete argument: " << arg << "\n";
    }
  }
  if (replInfo.role.empty()) {
        replInfo.role = "master";
        masterHost = "";
        masterPort = 0;
        connectedSlaves.clear();
        replInfo.master_replid = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
        replInfo.master_repl_offset = 0;
    }
}
void Server::run() {
  if(!dir.empty() && !dbfilename.empty()) loadRDBfile(dir,dbfilename);
  // boilerplate
  WSADATA wsaData;
  int wsaInit = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (wsaInit != 0) {
    std::cerr << "Winsock initialization failed" << std::endl;
    return;
  }
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == INVALID_SOCKET) {
    std::cerr << "Failed to create socket" << std::endl;
    WSACleanup();
    return;
  }
  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      SOCKET_ERROR) {
    std::cerr << "Bind failed" << std::endl;
    closesocket(server_fd);
    WSACleanup();
    return;
  }
  std::cout << "Bind successful" << std::endl;
  if (listen(server_fd, 5) == SOCKET_ERROR) {
    std::cerr << "Listen failed" << std::endl;
    closesocket(server_fd);
    WSACleanup();
    return;
  }
  
  char *ip_str = inet_ntoa(server_addr.sin_addr);
  std::cout << "Server is listening on " << ip_str << ":" << ntohs(server_addr.sin_port) << std::endl;
  
  if(replInfo.role == "slave"){sendHandshake();}

  std::vector<int> CLIENT_SOCKET_LIST;
  bool running = true;
  FD_SET clientSet;
  FD_ZERO(&clientSet);
  while(running){
    FD_ZERO(&clientSet);
    FD_SET(server_fd, &clientSet);
    for(int client_socket : CLIENT_SOCKET_LIST){
      FD_SET(client_socket, &clientSet);
    }
    int FILE_READY = select(0, &clientSet, NULL, NULL, NULL);
    if(FILE_READY > 0){
      if(FD_ISSET(server_fd, &clientSet)){ //checks if server_fd part of clientSet & ready to be read/written (if it has been marked by SELECT)
        sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) {
          std::cerr << "Accept failed" << std::endl;
          closesocket(server_fd);
          WSACleanup();
          return; //maybe add some slightly better error handling here
        }
        std::cout << "Client added:" << client_socket << std::endl;
        CLIENT_SOCKET_LIST.push_back(client_socket);
        FD_SET(client_socket, &clientSet);
      }
      for(int i = CLIENT_SOCKET_LIST.size()-1; i >= 0; i--){
        int client_socket = CLIENT_SOCKET_LIST[i];
        if(FD_ISSET(CLIENT_SOCKET_LIST[i], &clientSet))
        {
          char buffer[1024] = {0};
          int data_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0); // num of bytes
          if (data_received <= 0) { //means TCP connection closed gracefully
            CLIENT_SOCKET_LIST.erase(CLIENT_SOCKET_LIST.begin() + i);
            FD_CLR(client_socket, &clientSet);
            closesocket(client_socket);
            break;
          }
          std::vector<std::string> commands = generateCommands(buffer);
          std::cout << "Generated commands are: " << std::endl;
          for (const auto& command : commands) {
            std::cout << command << std::endl;
          }
          sendData(commands, client_socket);
        }
      }
    }
  }
  closesocket(server_fd);
  WSACleanup();
  return;
}

void toLowercase(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
}

void Server::sendData(const std::vector<std::string> &commands, int client_socket){
  int number_of_commands = commands.size();
  if(number_of_commands == 0)
  {
    std::cerr << "ERROR: Invalid Command" << std::endl;
  }
  std::string mainCommand = commands[0];
  std::stringstream return_data;
  return_data.str("");
  return_data.clear();
  toLowercase(mainCommand);
  if(mainCommand == "echo"){
    return_data << "$";
    return_data << std::to_string(commands[1].length());
    return_data << "\r\n";
    return_data << commands[1];
    return_data << "\r\n";
    std::cout << return_data.str();
    std::string response = return_data.str();
    send(client_socket, response.c_str(), response.length(), 0);
  }
  else if(mainCommand == "info"){
    if(commands.size() < 2){
      std::string response = "-ERR wrong number of arguments for 'INFO'\r\n";
      send(client_socket, response.c_str(), response.length(), 0);
      return;
    }
    std::string secondaryCommand = commands[1];
    toLowercase(secondaryCommand);
    if(secondaryCommand == "replication"){
      std::string data = "role:" + replInfo.role + "\r\nmaster_replid:" + replInfo.master_replid + "\r\nmaster_repl_offset:" + std::to_string(replInfo.master_repl_offset) + "\r\n";
      std::stringstream return_data;
      return_data << "$" << data.length()<< "\r\n" << data << "\r\n";
      std::string response = return_data.str();
      std::cout << response;
      send(client_socket, response.c_str(), response.length(), 0);
    }
  }
  else if(mainCommand == "ping"){
    //add the client socket to the handshake map?
    replicaHandshakeMap[client_socket] = Handshake_Stage::PING;
    std::string response = "+PONG\r\n";
    send(client_socket, response.c_str(), response.length(),0);
  }
  else if(mainCommand == "set"){
    if(commands.size() < 3){
      std::string response = "-ERR wrong number of arguments for 'SET'\r\n";
      send(client_socket, response.c_str(), response.length(), 0);
      return;
    }
    try{
      std::string optionalCommand = commands[3]; toLowercase(optionalCommand);
      if(commands.size() >= 5 && std::stoi(commands[4]) > 0 && optionalCommand == "px"){//with expiry
        std::cout << "Setting key with expiry: " 
                << "Key: " << commands[1] 
                << ", Value: " << commands[2] 
                << ", Expiry: " << commands[4] << "ms" 
                << std::endl; 
        setKey(commands[1],commands[2],commands[4]);
      }
      else{ // no expiry
        setKey(commands[1],commands[2],std::to_string(-1)); // -1 -> special value, if -1, treated as having no ttl
      }
     std::string response = "+OK\r\n"; 
     send(client_socket, response.c_str(), response.length(),0);
    }
    catch(const std::invalid_argument&){
      std::string response = "-ERR invalid PX arguments for 'SET'\r\n";
      send(client_socket, response.c_str(), response.length(),0);
    }
  }
  else if(mainCommand == "get"){
    if(commands.size() < 2){
      std::string response = "-ERR wrong number of arguments for 'GET'\r\n";
      send(client_socket, response.c_str(), response.length(), 0);
      return;
    }
    std::string value = getKeyValue(commands[1]);
    if(value.length() != 0){
      return_data.str("");
      return_data.clear();
      return_data << "$";
      return_data << std::to_string(value.length());
      return_data << "\r\n";
      return_data << value;
      return_data << "\r\n";
      std::cout << return_data.str();
      std::string response = return_data.str();
      send(client_socket, response.c_str(), response.length(), 0);
    }
    else{
      std::string response = "$-1\r\n";
      send(client_socket, response.c_str(), response.length(),0);
    }
  }
  else if(mainCommand == "command"){ //temp. fix for the redis-cli not wanting to connect
    if(commands.size() < 2){
      std::string response = "-ERR wrong number of arguments for 'GET'\r\n";
      send(client_socket, response.c_str(), response.length(), 0);
      return;
    }
    else{
      std::string response = "+OK\r\n";
      send(client_socket, response.c_str(), response.length(),0);
    }
  }
  else if(mainCommand == "keys"){
    if(commands.size() < 2){
      std::string response = "-ERR Wrong number of arguments for 'KEYS'\r\n";
      send(client_socket, response.c_str(), response.length(),0);
      return;
    }
    else{ //get all keys 
      size_t numkeys = dataStore.size();
      return_data.str("");
      return_data.clear();
      return_data << "*" << std::to_string(numkeys) << "\r\n" ;
      for(const auto& keyStruct : dataStore){
        return_data << "$" << std::to_string(keyStruct.first.length()) << "\r\n" << keyStruct.first << "\r\n"; 
      }
      std::string response = return_data.str();
      send(client_socket, response.c_str(), response.length(), 0);
    }
  }
  else if(mainCommand == "config"){
    if(commands.size() < 3){
     std::string response = "-ERR wrong number of arguments for 'CONFIG'\r\n";
      send(client_socket, response.c_str(), response.length(), 0);
      return;
    }
    else{
      std::string paramCommand = commands[1];
      std::string toGet = commands[2];
      toLowercase(paramCommand);
      toLowercase(toGet);
      return_data.str("");
      return_data.clear();
      if(paramCommand == "get"){
        return_data << "*2\r\n";
        return_data << "$";
        return_data << std::to_string(toGet.size());
        return_data << "\r\n";
        return_data << toGet;
        return_data << "\r\n";
        return_data << "$";
        if(toGet == "dir"){
          return_data << std::to_string(dir.length()) << "\r\n" << dir <<"\r\n";
        }
        else if(toGet == "dbfilename"){
          return_data << std::to_string(dbfilename.length()) << "\r\n" << dbfilename << "\r\n";
        }
        std::string response = return_data.str();
        send(client_socket, response.c_str(), response.length(), 0);
      }
    }
  }
  else if(mainCommand == "replconf"){
    if(commands.size() < 3){
      std::string response = "-ERR Replication ERROR - NOT ENOUGH ARGUMENTS\r\n";
      send(client_socket,response.c_str(),response.length(),0);
      return;
    }
    else{
      std::string command1 = commands[1];
      std::string command2 = commands[2];
      toLowercase(command1);
      toLowercase(command2);
      try{
        if(command1 == "listening-port" && replicaHandshakeMap.find(client_socket) != replicaHandshakeMap.end() && replicaHandshakeMap[client_socket] == Handshake_Stage::PING){ //handshake pt1
          std::string response = "+OK\r\n";
          send(client_socket, response.c_str(), response.length(),0);
          replicaHandshakeMap[client_socket] = Handshake_Stage::REPLCONF1;
          return;
        }
        else if(command1 == "capa" && replicaHandshakeMap.find(client_socket) != replicaHandshakeMap.end() &&replicaHandshakeMap[client_socket] == Handshake_Stage::REPLCONF1){ //handshake pt2
          std::string response = "+OK\r\n";
          send(client_socket, response.c_str(), response.length(),0);
          replicaHandshakeMap[client_socket] = Handshake_Stage::REPLCONF2;
          return;
        }
        else{
          std::cerr << "INVALID HANDSHAKE FROM FOLLOWER DB";  
          return;
        }
      }
      catch(const std::invalid_argument&){
        std::string response = "-ERR Replication ERROR - NOT ENOUGH ARGUMENTS\r\n";
        send(client_socket,response.c_str(),response.length(),0);
        return;
      }
    }
  } 
  else if(mainCommand == "psync"){
    if(replInfo.role != "master" || commands.size() < 3){
      std::string response = "-ERR Invalid command\r\n";
      send(client_socket, response.c_str(), response.length(), 0);
      std::cout << "Error response sent(PSYNC)\n";
      return;
    } 
    if(commands[1] == "?" && commands[2] == "-1" && replicaHandshakeMap.find(client_socket) != replicaHandshakeMap.end() && replicaHandshakeMap[client_socket] == Handshake_Stage::REPLCONF2){ //must be slave
      return_data.str("");
      return_data.clear();
      return_data << "+FULLRESYNC " << replInfo.master_replid << " 0\r\n";
      send(client_socket, return_data.str().c_str(), return_data.str().length(),0);
      replicaHandshakeMap[client_socket] = Handshake_Stage::PSYNC;
      //for testing purposes
      for (const auto& [socket, stage] : replicaHandshakeMap) {
      std::cout << "Socket: " << socket << " -> Handshake Stage: " << static_cast<int>(stage) << '\n';
      }
      std::string emptyFile = "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";
      return_data.str("");
      return_data.clear();
      return_data << "$" << std::to_string(emptyFile.size()) << "\r\n" << emptyFile;
      std::string secondResponse = return_data.str();
      send(client_socket, secondResponse.c_str(), secondResponse.length(),0);
      return;
    }
    else{
     std::string response = "-ERR Invalid arguments for command PSYNC\r\n";
      send(client_socket, response.c_str(), response.length(), 0);
      std::cout << "Error response sent(PSYNC args)\n";
      return;
    }
  }
  else{
    std::cout << "Sending error response to client\n";
    std::string response = "-ERR Invalid command\r\n";
    send(client_socket, response.c_str(), response.length(), 0);
    std::cout << "Error response sent\n";
    return;
  }
}

std::string Server::handleIndividualWord(const char charBuffer[1024], int* wordIndex)
{
  std::stringstream builder;
  ++(*wordIndex);
  std::string cmdLen;
  while(charBuffer[(*wordIndex)] != '\r' && *wordIndex < 1024)
  {
    cmdLen += charBuffer[(*wordIndex)];
    ++(*wordIndex);
  }
  int dataLength = std::stoi(cmdLen);
  ++(*wordIndex); 
  ++(*wordIndex); //eliminates need to skip r and n in loop
  for(size_t i = 0; i < dataLength; i++){
      builder << charBuffer[*wordIndex];
      ++(*wordIndex);
  }
  ++(*wordIndex);
  ++(*wordIndex); 
  std::cout << "Individual word: " << builder.str() << std::endl;
  return builder.str();
}
  
std::vector<std::string> Server::generateCommands(const char charBuffer[1024]){
  std::cout << "Received Buffer: \n" << charBuffer << std::endl;
  std::vector<std::string> commandList;
  if(charBuffer[0] == '*'){

    size_t i = 1;
    std::string bufferCmdLen;
    while (charBuffer[i] != '\r' && i < 1024) {
      bufferCmdLen += charBuffer[i];
      ++i;
    }
    int commandLength = std::stoi(bufferCmdLen);

    int* wordIndex = new int(i+2); //skip return and newline
    while(commandList.size() < commandLength){
     commandList.push_back(handleIndividualWord(charBuffer, wordIndex));
    }
    delete wordIndex;
  }
  return commandList;
}
void Server::sendHandshake(){ //so this is done when --replicaof flag is detected basically
  sockaddr_in master_addr;
  master_addr.sin_family = AF_INET;
  master_addr.sin_port = htons(masterPort);
  if (InetPton(AF_INET, masterHost.c_str(), &master_addr.sin_addr) != 1) {
        std::cerr << "Invalid IP address: " << masterHost << std::endl;
        return;
  }
  SOCKET master_fd = socket(AF_INET, SOCK_STREAM,0);
  if (master_fd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        return;
  }
  if(connect(master_fd, (struct sockaddr*)&master_addr, sizeof(master_addr))==0){
    std::cout <<"Connection succeeded.";
    std::string PING = "*1\r\n$4\r\nPING\r\n";
    std::string REPLCONF_1 = "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$4\r\n" + std::to_string(port) + "\r\n";
    std::string REPLCONF_2 = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n"; //temporarily (?) hardcoded
    std::string PSYNC = "*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n";
    send(master_fd, PING.c_str(), PING.size(), 0);
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &master_addr.sin_addr, ipStr, INET_ADDRSTRLEN);
    std::cout << "Sent PING to " << ipStr << " at port " << masterPort << std::endl;
    char buffer[1024];
    //ping
    int bytesReceived = recv(master_fd, buffer, sizeof(buffer)-1,0);
    if(bytesReceived > 0){
      std::string response(buffer,bytesReceived);
      //std::cout << buffer;
      if(response =="+PONG\r\n"){std::cout <<"PING - OK\r\n";}
      else{
        std::cerr << "ERROR: OK not received from master.";
        return;
      }
    }
    else{std::cerr << "INVALID RESPONSE FROM MASTER"; return;}
   
    //replconf1
    send(master_fd, REPLCONF_1.c_str(), REPLCONF_1.size(),0);
    bytesReceived = recv(master_fd, buffer, sizeof(buffer)-1,0);
    if(bytesReceived > 0){
      std::string response(buffer,bytesReceived);
      if(response =="+OK\r\n"){std::cout <<"REPLCONF 1 - OK\r\n";}
      else{
        std::cerr << "ERROR: OK not received from master.";
        return;
      }
    }
    else{
    std::cerr << "INVALID RESPONSE FROM MASTER";
      return;
    }
    //replconf2
    send(master_fd, REPLCONF_2.c_str(), REPLCONF_2.size(),0);
    bytesReceived = recv(master_fd, buffer, sizeof(buffer)-1,0);
    if(bytesReceived > 0){
      std::string response(buffer,bytesReceived);
      if(response =="+OK\r\n"){std::cout <<"REPLCONF 2 - OK\r\n";}
      else{
        std::cerr << "ERROR: OK not received from master."; 
        return;
      }
    }
    else{
      std::cerr << "INVALID RESPONSE FROM MASTER"; 
      return;
    }

    //psync
    send(master_fd, PSYNC.c_str(), PSYNC.size(),0);
    bytesReceived = recv(master_fd, buffer, sizeof(buffer)-1,0);
    if(bytesReceived > 0){
      std::string response(buffer,bytesReceived);
      if(response.substr(0,11) == "+FULLRESYNC" && response.size() == 56){ //len of fullresync + _replid_ + offset\r\n
        std::cout << "PSYNC - OK\r\n";
        std::cout << "HANDSHAKE COMPLETE\r\n";
        return;
      }
      else{
        std::cerr << "ERROR: OK not received from master."; 
        return;
      }
    }
    else{
      std::cerr << "INVALID RESPONSE FROM MASTER";
      return;
    }
    bytesReceived = recv(master_fd, buffer, sizeof(buffer)-1,0);
    if(bytesReceived > 0){ // this doesnt work for some reason lol
      std::string response(buffer,bytesReceived);
      std::cout << response;
      //fix parsing here
      if(response == "$176\r\n524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2"){
        std::cout << "EMPTY RDB TRANSFER COMPLETE\r\n";
      }
    }
    else{
      std::cout << "didnt receive empty file";
    }
  }
  else{
    std::cerr << "ERROR: Could not connect to master, is the master server running? Error: " << WSAGetLastError() << std::endl;
  }
}




