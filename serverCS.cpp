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
#include <fstream>
#include <unordered_map>

using std::string;

const string COURSE_FILE = "cs.txt";

template<typename K, typename V>
void print_map(std::unordered_map<K, V> const &m)
{
    for (auto const &pair: m) {
        std::cout << "{" << pair.first << ": " << pair.second << "}\n";
    }
}

int main(int argc, char *argv[]) {
    // Booting Up (Only while starting)
    std::cout << "The ServerCS is up and running using UDP on port " << PORT_SERVER_CS << "." << std::endl;

    // Create database for course information
    std::unordered_map<string, string> course_db;
    std::ifstream course_file (COURSE_FILE);
    if (course_file.is_open()) {
        string line;
        while (getline(course_file, line)) {
            parse_course_info(line, course_db);
        }
        course_file.close();
    } else {
        std::cerr << "ERROR: serverCS cannot open file `" << COURSE_FILE << "`!" << std::endl;
        exit(-1);
    }

#ifdef __DEBUG__
    print_map(course_db);
#endif

    // Setup UDP server
    int sockfd;
    setup_server_socket(sockfd, SOCK_DGRAM, PORT_SERVER_CS);

    while (true) {
        // Phase 3B
        // After getting the query information, the department server would look through its stored 
        // local information to obtain the corresponding course information.
        string request, request_header, request_payload;
        udp_recv_msg(sockfd, request);
        decode_msg(request, request_header, request_payload);
        if (request_header != QUERY_REQUEST) {
            // Just to be consistent with the message structure.
            // In the test case, this is not expected to happen.
            std::cerr << "serverC got wrong `auth_header`, expected `" << QUERY_REQUEST;
            std::cerr << "`, but received `" << request_header << "`." << std::endl;
        }
        
        string course_code, category;
        decode_msg(request_payload, course_code, category);
        std::cout << "The ServerCS received a request from the Main Server about the " << category << " of " << course_code << "." << std::endl;

        std::unordered_map<string, string>::iterator iter = course_db.find(course_code);
        string query_result_msg, query_result_payload;
        if (iter == course_db.end()) {
            std::cout << "Didn't find the course: " << course_code << "." << std::endl;
            query_result_payload = encode_msg(NO_COURSE_CODE, "dummy info");
        } else {  // information found
            string credit, professor, days, course_name;
            string info;
            parse_course_detail(iter->second, credit, professor, days, course_name);
            if (category == CREDIT) {
                info = credit;
            } 
            else if (category == PROF) {
                info = professor;
            }
            else if (category == DAYS) {
                info = days;
            }
            else if (category == COURSE_NAME) {
                info = course_name;
            } else {
                std::cout << "Could not find the information " << category << " of " << course_code << ", please check your spelling!" << std::endl;
                continue;
            }
            query_result_payload = encode_msg(FOUND_COURSE_INFO, info);
            std::cout << "The course information has been found: The " << category << " of " << course_code << " is " << info << std::endl;
        }

        query_result_msg = encode_msg(QUERY_RESULT, query_result_payload);
        udp_send_msg(sockfd, ADDR_SERVER_M, PORT_SERVER_M_UDP, query_result_msg);

        // After sending the results to the main server
        std::cout << "The ServerCS finished sending the response to the Main Server." << std::endl;

    }

    close(sockfd);

    return 0;
}