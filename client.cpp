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
#include <assert.h>

#define MAX_ATTEMPTS 3


/* Set up a TCP socket with dynamic port
 * Refer to https://beej.us/guide/bgnet/html/#a-simple-stream-client
 */
int setup_client_socket(int & sockfd, std::string host, std::string port) {
    struct addrinfo hints, *servinfo, *p;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
        std::cerr << "client: getaddrinfo(): " << gai_strerror(status) << std::endl;
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket()");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect()");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        std::cerr << "client: failed to connect" << std::endl;
        return 2;
    }

    return SUCCESS;
}

std::string encode_user_login(std::string & username) {
    std::cout << "Please enter the username: ";
    std::cin >> username;
    // count num of characters in username
    std::string num_chars_username = std::to_string(username.length());

    std::string password;
    std::cout << "Please enter the password: ";
    std::cin >> password;

    std::string username_password_pair = username + password;
    std::string encoded_msg = encode_msg(num_chars_username, username_password_pair);
    return encoded_msg;
}

std::string encode_course_query(std::string & course_code, std::string & category, std::string auth_token) {
    std::cout << "Please enter the course code to query: ";
    std::cin >> course_code;

    std::cout << "Please enter the category (Credit / Professor / Days / CourseName): ";
    std::cin >> category;

    std::string encoded_data = encode_msg(course_code, category);
    std::string encoded_msg = encode_msg(auth_token, encoded_data);
    return encoded_msg;
}

int main(int argc, char *argv[]) {

    // Booting Up
    int attempt = 0;
    std::string username = "";  // username for future fetching
    std::string auth_credential = "";  // token for course query
    std::string auth_result_code = FAIL_NO_USER;  // initialization
    std::cout << "The client is up and running." << std::endl;

    // Authentication Stage
    while (attempt < MAX_ATTEMPTS) {
        attempt++;

        int sockfd;
        if (setup_client_socket(sockfd, ADDR_SERVER_M, PORT_SERVER_M_TCP) != SUCCESS) {
            std::cerr << "client failed to connect to serverM" << std::endl;
            exit(1);
        }

        // Phase 1A
        // Client sends the authentication request to the main server over TCP connection
        std::string auth_request_payload = encode_user_login(username);
        std::string auth_request = encode_msg(AUTH_REQUST, auth_request_payload);
        tcp_send_msg(sockfd, auth_request);
        std::cout << username << " sent an authentication request to the main server." << std::endl;

        // Get authentication result
        std::string auth_result, auth_header, auth_payload;
        tcp_recv_msg(sockfd, auth_result);
        decode_msg(auth_result, auth_header, auth_payload);

        if (auth_header != AUTH_RESULT) {  
            // Just to be consistent with the message structure.
            // In the test case, this is not expected to happen.
            std::cerr << "client got wrong `auth_header`, expected `" << AUTH_RESULT;
            std::cerr << "`, but received `" << auth_header << "`." << std::endl;
            close(sockfd);
            exit(1);
        }
        unsigned int tcp_port = get_local_dynamic_port(sockfd);
        std::cout << username << " received the result of authentication using TCP over port " << tcp_port << ". ";
        decode_msg(auth_payload, auth_result_code, auth_credential);

        if (auth_result_code == FAIL_NO_USER) {
            std::cout << "Authentication failed: Username Does not exist" << std::endl;
            std::cout << "Attempts remaining:" << MAX_ATTEMPTS - attempt << std::endl;
            close(sockfd);
            continue;
        } else if (auth_result_code == FAIL_PASS_NO_MATCH) {
            std::cout << "Authentication failed: Password does not match" << std::endl;
            std::cout << "Attempts remaining:" << MAX_ATTEMPTS - attempt << std::endl;
            close(sockfd);
            continue;
        } else {
            // else: `auth_result_code` should be `PASS`
            assert(auth_result_code == PASS);  // sanity check
            std::cout << "Authentication is successful" << std::endl;
            close(sockfd);
            break;
        }
    }

    // Exceed maximum attempts, shutting down
    if (attempt == MAX_ATTEMPTS && auth_result_code != PASS) {
        std::cout << "Authentication Failed for " << MAX_ATTEMPTS << " attempts. Client will shut down." << std::endl;
        exit(1);
    }

    // Query Stage
    while (true) {
        int sockfd;
        if (setup_client_socket(sockfd, ADDR_SERVER_M, PORT_SERVER_M_TCP) != SUCCESS) {
            std::cerr << "client failed to connect to serverM" << std::endl;
            exit(1);
        }

        // Course query request
        std::string course_code, category;
        std::string query_request_payload = encode_course_query(course_code, category, auth_credential);
        std::string query_request = encode_msg(QUERY_REQUEST, query_request_payload);
        tcp_send_msg(sockfd, query_request);
        std::cout << username << " sent a request to the main server." << std::endl;

        // Query result
        std::string query_result, query_header, query_payload;
        tcp_recv_msg(sockfd,  query_result);
        unsigned int query_tcp_port = get_local_dynamic_port(sockfd);
        std::cout << "The client received the response from the Main server using TCP over port " << query_tcp_port << "." << std::endl;
        decode_msg(query_result, query_header, query_payload);
        if (query_header != QUERY_RESULT) {  
            // Just to be consistent with the message structure.
            // In the test case, this is not expected to happen.
            std::cerr << "client got wrong `query_header`, expected `" << QUERY_RESULT;
            std::cerr << "`, but received `" << query_header << "`." << std::endl;
            close(sockfd);
            exit(1);
        }
        std::string query_result_code, query_result_info;
        decode_msg(query_payload, query_result_code, query_result_info);

        if (query_result_code == NO_COURSE_CODE) {
            std::cout << "Didn’t find the course: " << course_code << "." << std::endl;
        } else {
            assert(query_result_code == FOUND_COURSE_INFO);
            std::cout << "The " << category << " of " << course_code << " is " << query_result_info << "." << std::endl;
        }

        std::cout << std::endl;
        std::cout << "-----Start a new request-----" << std::endl;
        close(sockfd);
    }

    return 0;
}