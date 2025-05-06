#include <iostream>
#include <string>
#include <winsock2.h>
#include <vector>
#include <algorithm>
#include <sstream>
//prototypes
//
std::vector<std::string> generateCommands(const char charBuffer[1024]);
std::string handleIndividualWord(const char charBuffer[1024], int* wordIndex);
void sendData(std::vector<std::string> commands, int client_socket);

int main() {
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
    //std::cout << FILE_READY;
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

void sendData(std::vector<std::string> commands, int client_socket){
  int number_of_commands = commands.size();
  if(number_of_commands == 0)
  {
    std::cerr << "ERROR: Invalid Command" << std::endl;
  }
  std::string mainCommand = commands[0];
  std::stringstream return_data;
  if(mainCommand == "ECHO")
  {
    return_data << "$";
    return_data << std::to_string(commands[1].length());
    return_data << "\r\n";
    return_data << commands[1];
    return_data << "\r\n";
    std::cout << return_data.str();
    std::string response = return_data.str();
    send(client_socket, response.c_str(), response.length(), 0);
  }
}


std::string handleIndividualWord(const char charBuffer[1024], int* wordIndex)
{
  std::stringstream builder;
  std::cout << "char buffer in individual thingy: " << charBuffer << std::endl;
  std::cout << "Starting to process from char: " << charBuffer[*wordIndex]<< std::endl; 
  ++(*wordIndex);
  std::string cmdLen;
  while(charBuffer[(*wordIndex)] != '\r')
  {
    cmdLen += charBuffer[(*wordIndex)];
    std::cout << "CURRENT CHAR: " <<charBuffer[*wordIndex] << std::endl;
    ++(*wordIndex);
  }
  std::cout << "command length (individual word) : " << cmdLen << std::endl;
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
  std::cout << "Received Buffer: " << charBuffer << std::endl;
  std::vector<std::string> commandList;
  if(charBuffer[0] == '*'){

    int i = 1;
    std::string bufferCmdLen;
    while (charBuffer[i] != '\r' && i < 1024) {
      bufferCmdLen += charBuffer[i];
      ++i;
    }
    int commandLength = std::stoi(bufferCmdLen);

    int* wordIndex = new int(commandLength+2); //skip return and newline
    std::cout << "Command length is: " << commandLength << std::endl;
    while(commandList.size() < commandLength){
     std::cout << "CALLING INDIVIDUAL WORD FUNCTION" << std::endl;
     commandList.push_back(handleIndividualWord(charBuffer, wordIndex));
    }
    delete wordIndex;
  }
  return commandList;
}






