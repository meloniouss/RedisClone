#include <winsock2.h>
#include <iostream>
#include <string>
int main() {
    //boilerplate
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
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed" << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }
    if (listen(server_fd, 5) == SOCKET_ERROR) {
        std::cerr << "Listen failed" << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }



    
    //core logic
    std::cout << "Server is listening on port 6379..." << std::endl;

    sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Accept failed" << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }
    std::cout << "Client connected!" << std::endl;
    //actual core logic
    bool connectionOpen = true;
    int commandsReceived = 3;
    while(connectionOpen){
        char buffer[1024] = {0};
        int data_received = recv(client_socket, buffer, sizeof(buffer)-1,0); // num of bytes
        if(data_received <= 0){break;}
        std::cout << "Client message: " << "PING" << std::endl;
        send(client_socket, "PONG",strlen("PONG"),0);
    }

    closesocket(client_socket);
    closesocket(server_fd);
    WSACleanup();
    return 0;
}
