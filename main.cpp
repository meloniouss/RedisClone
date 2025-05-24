#include <winsock2.h>
#include <iostream>
#include <string>
#include "server.hpp"

//for future use maybe?
std::unordered_map<int, Server*> portMapping;
Server* masterServer;
std::vector<Server*> replicaList;
int main(int argc, char* argv[]) {
    Server server;
    server.generateVars(argc, argv);
    server.run();
    return 0;
}
