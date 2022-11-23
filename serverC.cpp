#include "convention.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <map>
#include <unordered_map>
#include <algorithm>

using std::string;

const string CRED_FILE = "cred.txt";  // TODO: Change this before submission

void bootup() {
    std::cout << "The ServerC is up and running using UDP on port " << PORT_SERVER_C << "." << std::endl;
}

template<typename K, typename V>
void print_map(std::unordered_map<K, V> const &m)
{
    for (auto const &pair: m) {
        std::cout << "{" << pair.first << ": " << pair.second << "}\n";
    }
}

int main(int argc, char *argv[]) {
    // Booting Up (Only while starting)
    bootup();

    // Creating database for encrypted username and password pair
    std::unordered_map<string, string> cred_db; 
    std::ifstream cred_file (CRED_FILE);
    if (cred_file.is_open()) {
        string line, key, value;
        while (getline(cred_file, line)) {
            std::istringstream lineStream(line);
            getline(lineStream, key, ',');
            getline(lineStream, value);
            // Remove annoying `\r` and `\n` from end of the value for string comparison sake :)
            value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
            value.erase(std::remove(value.begin(), value.end(), '\n'), value.end());
            cred_db[key] = value;
        }
        cred_file.close();
    } else {
        std::cerr << "ERROR: serverC cannot open file `" << CRED_FILE  << "`!" << std::endl;
        exit(-1);
    }

    // Setup UDP server
    int sockfd;  // Socket file descripter
    setup_server_socket(sockfd, SOCK_DGRAM, PORT_SERVER_C);

    while (true) {
        // Phase 2A
        // Once the authentication request is received (contain the encrypted form of the username and the password), 
        // the serverC should first check if the username in the authentication request matches with any of the usernames 
        // present in the cred.txt file.

        string request, request_header, request_payload;
        udp_recv_msg(sockfd, request);
        decode_msg(request, request_header, request_payload);
        if (request_header != AUTH_REQUST) {
            // Just to be consistent with the message structure.
            // In the test case, this is not expected to happen.
            std::cerr << "serverC got wrong `auth_header`, expected `" << AUTH_REQUST;
            std::cerr << "`, but received `" << request_header << "`." << std::endl;
        }
        // TODO: check whether the request comes from the main server?
        // Upon Receiving the request from main server
        std::cout << "The ServerC received an authentication request from the Main Server." << std::endl;

        string encrypted_username, encrypted_password;
        decode_auth_payload(request_payload, encrypted_username, encrypted_password);
        // If the username exists, it secondly checks if the password in the authentication request is the same as the 
        // password corresponding to the same username in the cred.txt file.
        std::unordered_map<string, string>::iterator iter = cred_db.find(encrypted_username);
        string auth_result, response_payload, result_code;
        string dummy_info = "";
        if (iter == cred_db.end()) {
            result_code = FAIL_NO_USER;
        }
        else if (encrypted_password != iter->second) {
            result_code = FAIL_PASS_NO_MATCH;
        }
        else {
            result_code = PASS;
        }

        response_payload = encode_msg(result_code, dummy_info);
        auth_result = encode_msg(AUTH_RESULT, response_payload);
        // If both the checks are passed then serverC sends an authentication pass message to the serverM using UDP.
        udp_send_msg(sockfd, ADDR_SERVER_M, PORT_SERVER_M_UDP, auth_result);

        // After sending the results to the main server
        std::cout << "The ServerC finished sending the response to the Main Server." << std::endl;
    }

    close(sockfd);

    return 0;
}