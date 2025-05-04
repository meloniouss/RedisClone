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
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        WSACleanup();
        return 1;
    }
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6379);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed" << std::endl;
        closesocket(client_fd);
        WSACleanup();
        return 1;
    }
    else{
        std::cout << "CONNECTED TO SERVER";
    }

    //this is where the interesting stuff happens
    //to-do: ask for user input until EXIT command or something
    bool isConnected = true;
    int numCommands = 3;
    while(isConnected && numCommands > 0){
        const char* message = "PING";
        send(client_fd, message, strlen(message), 0);
        std::cout << "Client sent: " << message << std::endl;

        // Receive response
        char buffer[1024] = {0};
        int bytesReceived = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            std::cout << "Server response: " << buffer << std::endl;
        }
        numCommands--;
    }
    // Close socket
    closesocket(client_fd);
    WSACleanup();
    return 0;
}
