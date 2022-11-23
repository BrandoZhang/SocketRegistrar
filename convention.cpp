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
#include <unordered_map>
#include <algorithm>


void parse_key_value_by(char deliminator, std::string origin_data, std::string & key, std::string & value) {
    std::istringstream line_stream(origin_data);
    getline(line_stream, key, deliminator);
    getline(line_stream, value);
    // Remove annoying `\r` and `\n` from end of the value for string comparison sake :)
    value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
    value.erase(std::remove(value.begin(), value.end(), '\n'), value.end());
}

std::string encode_msg(std::string header_type, std::string payload) {
    std::string encoded_msg = header_type + " " + payload;
    return encoded_msg;
}

void decode_msg(std::string encoded_msg, std::string & header, std::string & payload) {
    parse_key_value_by(' ', encoded_msg, header, payload);
}

void decode_auth_payload(std::string auth_payload, std::string & username, std::string & password) {
    std::string num_chars_username, username_password_pair; 
    decode_msg(auth_payload, num_chars_username, username_password_pair);
    username = username_password_pair.substr(0, std::stoi(num_chars_username));
    password = username_password_pair.substr(std::stoi(num_chars_username));
}

void parse_course_info(std::string origin_data, std::unordered_map<std::string, std::string> & course_db) {
    std::string course_code, course_info;
    std::istringstream line_stream(origin_data);
    getline(line_stream, course_code, ',');
    getline(line_stream, course_info);
    // Remove annoying `\r` and `\n` from end of the value for string comparison sake :)
    course_info.erase(std::remove(course_info.begin(), course_info.end(), '\r'), course_info.end());
    course_info.erase(std::remove(course_info.begin(), course_info.end(), '\n'), course_info.end());
    course_db[course_code] = course_info;
}

void parse_course_detail(std::string course_info, std::string & credit, std::string & professor, std::string & days, std::string & course_name) {
    std::istringstream line_stream(course_info);
    getline(line_stream, credit, ',');
    getline(line_stream, professor, ',');
    getline(line_stream, days, ',');
    getline(line_stream, course_name);
    // Remove annoying `\r` and `\n` from end of the value for string comparison sake :)
    course_name.erase(std::remove(course_name.begin(), course_name.end(), '\r'), course_name.end());
    course_name.erase(std::remove(course_name.begin(), course_name.end(), '\n'), course_name.end());
}
// // refer to Beej's socket tutorial
// void sigchld_handler(int s) {
//     // waitpid() might overwrite errno, so we save and restore it
//     int saved_errno = errno;

//     while(waitpid(-1, NULL, WNOHANG) > 0);

//     errno = saved_errno;
// }

// get sockaddr, IPv4 or IPv6, refer to Beej's socket tutorial
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

unsigned int get_local_dynamic_port(int sockfd) {
    unsigned int client_port;
    struct sockaddr_in client_address;
    bzero(&client_address, sizeof(client_address));
    socklen_t len = sizeof(client_address);
    getsockname(sockfd, (struct sockaddr *) &client_address, &len);
    client_port = ntohs(client_address.sin_port);
    return client_port;
}

int setup_server_socket(int & sockfd, const int socket_type, const std::string port) {
    struct addrinfo hints, *servinfo, *p;
    int status;
    int yes = 1;  // param for setsockopt() to reuse a port
    // char yes = '1';  // Solaris people use this

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // don't care IPv4 or IPv6
    hints.ai_socktype = socket_type;
    hints.ai_flags = AI_PASSIVE;

    // TODO: Why in server side we put NULL here?
    if ((status = getaddrinfo(NULL, port.c_str(), &hints, &servinfo) != 0)) {
        std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket()");  // TODO: error msg here
            continue;
        }

        // reuse the port to lose the pesky "Address already in use" error message
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
            perror("setsockopt()");  // TODO: error msg here
            exit(1);  // TODO: error code
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen)) {
            close(sockfd);
            perror("bind()");  // TODO: error msg here
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        std::cerr << "Failed to bind socket." << std::endl;
        exit(1);  // TODO: conventions
    }

    if (socket_type == USE_TCP && listen(sockfd, BACKLOG) == -1) {
        perror("listen()");  // TODO: error msg here
        exit(1);
    }

    return 0;
}

int tcp_send_msg(int sockfd, std::string msg) {
#ifdef __DEBUG__
    std::cout << "[DEBUG] Attempts to send: " << msg << std::endl;
#endif

    int numbytes;
    if ((numbytes = send(sockfd, msg.c_str(), msg.length(), 0)) == -1) {
        perror("send()");
        exit(1);
    }

    if (numbytes < (int) msg.length()) {
        std::cerr << "Expected to send " << msg.length() << " bytes, but only " << numbytes << " bytes are sent through TCP." << std::endl;
        return 3;  // TODO: error code
    }

    return SUCCESS;
}

int tcp_recv_msg(int sockfd, std::string & msg) {
    int numbytes;
    char buf[MAXBUFLEN];

    if ((numbytes = recv(sockfd, buf, MAXBUFLEN - 1, 0)) == -1) {
        perror("recv()");
        exit(1);  // TODO: exit code here
    }

    buf[numbytes] = '\0';
    msg = buf;

#ifdef __DEBUG__
    // Refer to Beej's tutorial on `getpeername()`
    char ipstr[INET6_ADDRSTRLEN]; 
    int port;
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    getpeername(sockfd, (struct sockaddr *)&their_addr, &addr_len);
    if (their_addr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&their_addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
    } else {  // AF_INET6
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&their_addr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
    }
    std::cout << "------" << std::endl;
    std::cout << "[DEBUG] Got TCP segment from " << ipstr << std::endl;
    std::cout << "[DEBUG] Remote port is " << port << std::endl;
    std::cout << "[DEBUG] Packet size: " << numbytes << std::endl;
    std::cout << "[DEBUG] Packet content: " << msg << std::endl;
    std::cout << "------" << std::endl;
#endif

    return SUCCESS;
}

int udp_send_msg(int sockfd, std::string host, std::string port, std::string msg) {
    struct addrinfo hints, *servinfo, *p;
    int status;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;  // UDP
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(host.c_str(), port.c_str(), &hints, &servinfo) != 0)) {
        std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
        return 1;
    }

#ifdef __DEBUG__
    std::cout << "[DEBUG] Attempts to send: " << msg << std::endl;
#endif

    // loop through all the results and send via the first one we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((numbytes = sendto(sockfd, msg.c_str(), msg.length(), 0, p->ai_addr, p->ai_addrlen)) == -1) {
            perror("sendto");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        std::cerr << "Failed to send the message." << std::endl;
        return 2;  // TODO: error code
    }

    if (numbytes < (int) msg.length()) {
        std::cerr << "Expected to send " << msg.length() << " bytes, but only " << numbytes << " bytes are sent through UDP." << std::endl;
        return 3;  // TODO: error code
    }

    return 0;
}

int udp_recv_msg(int sockfd, std::string & msg) {
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len = sizeof their_addr;

    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom()");  // TODO: error msg here
        exit(1);  // TODO: exit code here
    }
    buf[numbytes] = '\0';
    msg = buf;

#ifdef __DEBUG__
    char s[INET6_ADDRSTRLEN];  // can support both IPv4 and IPv6
    std::cout << "------" << std::endl;
    std::cout << "[DEBUG] Got UDP datagram from " << inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s) << std::endl;
    std::cout << "[DEBUG] Remote port is " << ntohs(((struct sockaddr_in *)&their_addr)->sin_port) << std::endl;
    std::cout << "[DEBUG] Packet size: " << numbytes << std::endl;
    std::cout << "[DEBUG] Packet content: " << msg << std::endl;
    std::cout << "------" << std::endl;
#endif
    return 0;
}