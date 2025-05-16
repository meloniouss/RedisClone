#include <iostream>
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
//prototypes
std::vector<std::string> generateCommands(const char charBuffer[1024]);
std::string handleIndividualWord(const char charBuffer[1024], int* wordIndex);
void sendData(const std::vector<std::string> &commands, int client_socket);
std::string fetchKeys(std::string key);
struct keyInfo {
  std::string value;
  std::chrono::system_clock::time_point timestamp;
  std::chrono::milliseconds ttl;
};

static std::unordered_map<std::string, keyInfo> dataStore;

void setKey(std::string key, std::string value, std::string ttl)
{
  keyInfo key_Value{
  value,
  std::chrono::system_clock::now(),
  std::chrono::milliseconds(std::stoi(ttl))
  };
  dataStore.emplace(key, key_Value);
}

std::string getKeyValue(std::string key){
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

std::string dir;
std::string dbfilename;

//create a struct for this
size_t getSize(std::string* fileContents, size_t* cursor){   
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
      for (int i = 0; i < 4; ++i){
        size <<= 8;
        size |= static_cast<uint8_t>((*fileContents)[(*cursor)++]);
      }
    }
    else if(firstByte == 0x81){ //read next 8 bytes
      for (int i = 0; i < 8; ++i) {
        size <<= 8;
        size |= static_cast<uint8_t>((*fileContents)[(*cursor)++]);
      }
    }
  }
  else if(twoFirstBits == 0b11){ //specific string encoding
    // !!! TO-DO !!!
    size = (firstByte); //might honestly be worth adding a struct here called RDBEncoding or something.
    (*cursor)++;
  }
  return size;
}
void handleDbRead(std::string* fileContents, size_t* cursor){
  auto dbIndex = getSize(fileContents, cursor);
  auto numKeys = 0;
  auto numExpKeys = 0;

  while(static_cast<unsigned char>((*fileContents)[*cursor]) != 0xFB){(*cursor)++;}
  
  numKeys = getSize(fileContents,cursor);
  numExpKeys = getSize(fileContents,cursor);
  int i = 0;
  auto ttl = std::chrono::milliseconds(0);
  std::stringstream keyStrStream;
  std::stringstream valueStrStream;
  size_t keyLength = 0;
  size_t valueLength = 0;
 
  while(i < numKeys){ //read until we have all keys
    if(!valueStrStream.str().empty() && valueStrStream.str().length() == valueLength){
      ttl = (ttl == std::chrono::milliseconds(0)) ? std::chrono::milliseconds(-1): ttl;
      // !!! TO-DO !!!
      setKey(keyStrStream.str(), valueStrStream.str(), std::to_string(ttl.count())); //we need to convert either to ascii or to (integer to string)
      ttl = std::chrono::milliseconds(0);
      keyStrStream.str("");
      keyStrStream.clear();
      valueStrStream.str("");
      valueStrStream.clear();
      keyLength = 0;
      valueLength = 0;
      i++;
    }
    else if(static_cast<unsigned char>((*fileContents)[(*cursor)++]) == 0xFC){ //milliseconds
      uint64_t expirationRaw = 0;
      for(int j = 0; j < 8; j++){
        expirationRaw |= static_cast<uint64_t>(static_cast<unsigned char>((*fileContents)[(*cursor)++])) << (8 * j);
        (*cursor)++;
      }
      auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
      ttl = std::chrono::milliseconds(expirationRaw) - now_ms;
    }
    else if(static_cast<unsigned char>((*fileContents)[*cursor]) == 0xFD){ //seconds transformed into milliseconds
      uint32_t expireSeconds = 0;
      for(int j = 0; j < 4; j++) {
        expireSeconds |= (static_cast<uint32_t>(static_cast<unsigned char>((*fileContents)[*cursor])) << (8 * j));
        (*cursor)++;
      }
      auto expireMillis = std::chrono::milliseconds(expireSeconds * 1000ULL);
      ttl = expireMillis - std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    }
    else{
      (*cursor)++; //this is the value type -->> no use for it yet?
      if(keyStrStream.str().empty()){ //read key
        (*cursor)++; //skip value type
        keyLength = getSize(fileContents, cursor);
        for(int index = 0; index < keyLength; index++){
          keyStrStream <<static_cast<unsigned char>((*fileContents)[*cursor]);
          (*cursor)++;
        }
      }
      if(valueStrStream.str().empty()){ //read value
        valueLength = getSize(fileContents,cursor);
        if((valueLength >> 6) == 0b11) //special string encoding
        {
            // !!! TO DO !!! 
        }
        else{ //regular encoded
          for(int index = 0; index < valueLength; index++){
            valueStrStream <<static_cast<unsigned char>((*fileContents)[*cursor]);
            (*cursor)++;
          }
        }
      }
    }
  }
}
void loadRDBfile(std::string dir, std::string dbfilename){
  //all numbers are little endian (stored in reverse order)
  std::filesystem::path filepath = std::filesystem::path(dir) / dbfilename; 
  std::cout << filepath << std::endl;
  std::ifstream RDB_FILE(filepath, std::ios::binary);
  if(!RDB_FILE){
    std::cerr << "ERROR: Invalid RDB file, make sure to use an absolute path." << std::endl;
  }
  auto fileSize = std::filesystem::file_size(filepath);
  std::string fileContents(fileSize, '\0');
  RDB_FILE.read(fileContents.data(), fileSize);
  if(!RDB_FILE.read(fileContents.data(), fileSize)){
    std::cerr << "ERROR: Failed to read RDB file.\n";
  }
  size_t cursor = 0;
  while(cursor < fileContents.size()){
    if(fileContents.substr(cursor, 6) == "REDIS"){
      cursor += 10;
    }
    else if(static_cast<unsigned char>(fileContents[cursor]) == 0xFE){
      //handle db function -> pass by &
      handleDbRead(&fileContents, &cursor);
    }
    else if(static_cast<unsigned char>(fileContents[cursor]) == 0xFF){
      //eof
    }
    else{
      cursor++; //might be able to update cursor in if statements.
    }
  }
}


int main(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dir" && i + 1 < argc) {
            dir = argv[++i];
        } else if (arg == "--dbfilename" && i + 1 < argc) {
            dbfilename = argv[++i];
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
        }
    }

  if(!dir.empty() && !dbfilename.empty()) loadRDBfile(dir,dbfilename);
 
  // boilerplate
  WSADATA wsaData;
  int wsaInit = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (wsaInit != 0) {
    std::cerr << "Winsock initialization failed" << std::endl;
    return 1;
  }
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == INVALID_SOCKET) {
    std::cerr << "Failed to create socket" << std::endl;
    WSACleanup();
    return 1;
  }
  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      SOCKET_ERROR) {
    std::cerr << "Bind failed" << std::endl;
    closesocket(server_fd);
    WSACleanup();
    return 1;
  }
  std::cout << "Bind successful" << std::endl;
  if (listen(server_fd, 5) == SOCKET_ERROR) {
    std::cerr << "Listen failed" << std::endl;
    closesocket(server_fd);
    WSACleanup();
    return 1;
  }
  // core logic
  char *ip_str = inet_ntoa(server_addr.sin_addr);
  std::cout << "Server is listening on " << ip_str << ":" << ntohs(server_addr.sin_port) << std::endl;
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
          return 1; //maybe add some slightly better error handling here
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
  return 0;
}

void toLowercase(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
}

void sendData(const std::vector<std::string> &commands, int client_socket){
  int number_of_commands = commands.size();
  if(number_of_commands == 0)
  {
    std::cerr << "ERROR: Invalid Command" << std::endl;
  }
  std::string mainCommand = commands[0];
  std::stringstream return_data;
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
}

std::string handleIndividualWord(const char charBuffer[1024], int* wordIndex)
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
  for(int i = 0; i < dataLength; i++){
      builder << charBuffer[*wordIndex];
      ++(*wordIndex);
  }
  ++(*wordIndex);
  ++(*wordIndex); 
  std::cout << "Individual word: " << builder.str() << std::endl;
  return builder.str();
}
  
std::vector<std::string> generateCommands(const char charBuffer[1024]){
  std::cout << "Received Buffer: \n" << charBuffer << std::endl;
  std::vector<std::string> commandList;
  if(charBuffer[0] == '*'){

    int i = 1;
    std::string bufferCmdLen;
    while (charBuffer[i] != '\r' && i < 1024) {
      bufferCmdLen += charBuffer[i];
      ++i;
    }
    int commandLength = std::stoi(bufferCmdLen);

    int* wordIndex = new int(i+2); //skip return and newline
    //std::cout << "STARTING WORD INDEX IS: " << *wordIndex << std::endl;
    //std::cout << "CHAR AT SAID INDEX IS: " << charBuffer[*wordIndex] << std::endl;
    //std::cout << "Command length is: " << commandLength << std::endl;
    while(commandList.size() < commandLength){
     commandList.push_back(handleIndividualWord(charBuffer, wordIndex));
    }
    delete wordIndex;
  }
  return commandList;
}




