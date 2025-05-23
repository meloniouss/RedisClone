#include <winsock2.h>
#include <iostream>
#include <string>
#include "server.hpp"

int main(int argc, char* argv[]) {
    Server server;
    server.generateVars(argc, argv);
    server.run();
    return 0;
}
