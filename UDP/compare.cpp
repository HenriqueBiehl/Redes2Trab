#include <iostream> 
#include <fstream>
#include <filesystem>
#include <chrono>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#include "lib_network.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cout << "Formato correto: ./compare <nome-arquivo>" << endl;
        exit(1);
    }
    compare_file(argv[1]);
}