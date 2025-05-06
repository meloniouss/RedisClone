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
          std::cout << "Generated command are: " << std::endl;
          for (const auto& command : commands) {
            std::cout << command << std::endl;
          }
          std::cout << "Client ("<< client_socket<<") message: " << "PING" << std::endl;
          //
          // need to add data sending function here
          //
          send(client_socket, "PONG", strlen("PONG"), 0);
        }
      }
    }
  }
  closesocket(server_fd);
  WSACleanup();
  return 0;
}

std::string handleIndividualWord(const char charBuffer[1024], int* wordIndex)
{
  std::stringstream builder;
  std::cout << "Starting to process from char: " << charBuffer[*wordIndex]<< std::endl;
  int dataLength = charBuffer[*wordIndex + 1];
  for(int i = 0; i < dataLength; i++){
    (*wordIndex)++; //used to track position in the buffer
    if(charBuffer[i] == '\r'|| charBuffer[i] == '\n'){
      continue;
    }
    else{
      builder << charBuffer[i];
    }
  }
  std::cout << "Individual word: " << builder.str() << std::endl;
  return builder.str();
}
  
std::vector<std::string> generateCommands(const char charBuffer[1024]){
  std::cout << "Received Buffer: " << charBuffer << std::endl;
  std::vector<std::string> commandList;
  if(charBuffer[0] == '*'){
    int commandLength = (int) charBuffer[1];
    int* wordIndex = new int(4);// we skip *length\r\n so index is 4
    while(commandList.size() < commandLength){
     std::cout << "CALLING INDIVIDUAL WORD FUNCTION" << std::endl;
     commandList.push_back(handleIndividualWord(charBuffer, wordIndex));
    }
    delete wordIndex;
  }
  return commandList;
}






