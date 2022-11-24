#include "convention.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>
#include <unordered_map>

using std::string;

const int OFFSET_ENCRYPT = 4;


string cipher(string text, int shift) {
    for (std::size_t i = 0; i < text.length(); ++i) {
        // Encrypt lower case characters
        if (islower(text[i]))
            text[i] = char(int(text[i] + shift - int('a')) % 26 + int('a'));
        // Encrypt upper case characters
        else if (isupper(text[i]))
            text[i] = char(int(text[i] + shift - int('A')) % 26 + int('A'));
        // Encrypt digits
        else if (isdigit(text[i]))
            text[i] = char(int(text[i] + shift - int('0')) % 10 + int('0'));
    }
    return text;
}

string random_string(size_t length) {  // copied from https://stackoverflow.com/a/12468109
    auto randchar = []() -> char {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[rand() % max_index];
    };
    string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}

string generate_token() {
    string token = random_string(16);  // use a fix-size string as token
    return token;
}

void auth_routine(int tcp_child_sockfd, int udp_sockfd, string auth_payload, std::unordered_map<string, string> & authorized_tokens) {
    string username, password;
    decode_auth_payload(auth_payload, username, password);
    std::cout << "The main server received the authentication for " << username << " using TCP over port " << PORT_SERVER_M_TCP << "." << std::endl;

    // Phase 1B
    // Main server forwards the authentication request to the credentials server over UDP.
    string num_chars_username = std::to_string(username.length());
    string encrypted_username_password_pair = cipher(username, OFFSET_ENCRYPT) + cipher(password, OFFSET_ENCRYPT);
    string auth_request = encode_msg(num_chars_username, encrypted_username_password_pair);
    auth_request = encode_msg(AUTH_REQUST, auth_request);

    udp_send_msg(udp_sockfd, ADDR_SERVER_C, PORT_SERVER_C, auth_request);
    std::cout << "The main server sent an authentication request to serverC." << std::endl;
    
    // receive authentication result from serverC
    string auth_result, auth_header, auth_payload_C, auth_result_code, dummy_info;
    udp_recv_msg(udp_sockfd, auth_result);
    std::cout << "The main server received the result of the authentication request from ServerC using UDP over port " << PORT_SERVER_M_UDP << "." << std::endl;
    decode_msg(auth_result, auth_header, auth_payload_C);
    if (auth_header != AUTH_RESULT) {  
        // Just to be consistent with the message structure.
        // In the test case, this is not expected to happen.
        std::cerr << "serverM got wrong `auth_header`, expected `" << AUTH_RESULT;
        std::cerr << "`, but received `" << auth_header << "`." << std::endl;
    }
    decode_msg(auth_payload_C, auth_result_code, dummy_info);

    string outgoing_msg, outgoing_payload;
    if (auth_result_code == PASS) {
        string token = generate_token();
        authorized_tokens[token] = username;
        outgoing_payload = encode_msg(auth_result_code, token);
    } else {
        outgoing_payload = encode_msg(auth_result_code, dummy_info);
    }
    outgoing_msg = encode_msg(AUTH_RESULT, outgoing_payload);

    tcp_send_msg(tcp_child_sockfd, outgoing_msg);
    std::cout << "The main server sent the authentication result to the client." << std::endl;
}

void query_routine(int tcp_child_sockfd, int udp_sockfd, string query_payload, std::unordered_map<string, string> & authorized_tokens) {
    // Check if the client is authorized
    string token, query_data;
    decode_msg(query_payload, token, query_data);
    std::unordered_map<string, string>::iterator iter = authorized_tokens.find(token);
    if (iter == authorized_tokens.end()) {  // no authorized query
        // TODO: This case will not happen, but std::cout sth.
        return;
    }
    string username = iter->second;  // fetch username from authorized tokens
    string course_code, category;
    decode_msg(query_data, course_code, category);
    std::cout << "The main server received from " << username << " to query course " << course_code << " about " << category << " using TCP over port " << PORT_SERVER_M_TCP << "." << std::endl;
    
    // Check department
    string department = course_code.substr(0, 2);  // the department code
#ifdef __DEBUG__
    std::cout << "[DEBUG] Parsed `department`: " << department << std::endl;
#endif
    if (department != "EE" && department != "CS") {
        string query_result_payload = encode_msg(NO_COURSE_CODE, "dummy");
        string query_result = encode_msg(QUERY_RESULT, query_result_payload);
        tcp_send_msg(tcp_child_sockfd, query_result);
        std::cout << "The main server sent the query information to the client." << std::endl;
        return;
    }
    query_data = encode_msg(QUERY_REQUEST, query_data);
    if (department == "EE") {
        // Forward to serverEE
        udp_send_msg(udp_sockfd, ADDR_SERVER_EE, PORT_SERVER_EE, query_data);
        std::cout << "The main server sent a request to serverEE." << std::endl;
        // Received query result from serverEE
        string query_result;
        udp_recv_msg(udp_sockfd, query_result);
        std::cout << "The main server received the response from serverEE using UDP over port " << PORT_SERVER_M_UDP << "." << std::endl;
        tcp_send_msg(tcp_child_sockfd, query_result);  // directly forward
        std::cout << "The main server sent the query information to the client." << std::endl;
    }
    if (department == "CS") {
        // Forward to serverCS
        udp_send_msg(udp_sockfd, ADDR_SERVER_CS, PORT_SERVER_CS, query_data);
        std::cout << "The main server sent a request to serverCS." << std::endl;
        // Received query result from serverCS
        string query_result;
        udp_recv_msg(udp_sockfd, query_result);
        std::cout << "The main server received the response from serverCS using UDP over port " << PORT_SERVER_M_UDP << "." << std::endl;
        tcp_send_msg(tcp_child_sockfd, query_result);  // directly forward
        std::cout << "The main server sent the query information to the client." << std::endl;
    }
}

int setup_child_tcp_socket(int sockfd, int & child_sockfd) {  // Refer to https://beej.us/guide/bgnet/html/#a-simple-stream-server
    struct sockaddr_storage their_addr;  // connector's address information
    socklen_t sin_size = sizeof their_addr;
    int new_sockfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_sockfd == -1) {
        perror("serverM: accept()");
        exit(1);
    }
    child_sockfd = new_sockfd;
    return SUCCESS;
}

int main(int argc, char *argv[]) {
    // Booting Up (only while starting)
    std::unordered_map<string, string> authorized_tokens;  // {token: username}
    std::cout << "The main server is up and running." << std::endl;

    int udp_sockfd, tcp_sockfd, child_sockfd;
    setup_server_socket(udp_sockfd, SOCK_DGRAM, PORT_SERVER_M_UDP);
    setup_server_socket(tcp_sockfd, SOCK_STREAM, PORT_SERVER_M_TCP);

    while (true) {
        setup_child_tcp_socket(tcp_sockfd, child_sockfd);

        string incoming_request, incoming_header, incoming_payload;
        tcp_recv_msg(child_sockfd, incoming_request);
        decode_msg(incoming_request, incoming_header, incoming_payload);

        if (incoming_header == AUTH_REQUST) {
            auth_routine(child_sockfd, udp_sockfd, incoming_payload, authorized_tokens);
        }
        else if (incoming_header == QUERY_REQUEST) {
            query_routine(child_sockfd, udp_sockfd, incoming_payload, authorized_tokens);
        } else {
            // Unsupported TCP request type
        }

        close(child_sockfd);
    }

    close(udp_sockfd);
    close(tcp_sockfd);

    std::cout << "Exit..." << std::endl;
    return 0;
}
