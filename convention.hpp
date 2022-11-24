#ifndef _CONVENTION_H
#define _CONVENTION_H

// #define __DEBUG__  // flag to show debug message 

#include <string>
#include <unordered_map>

/* Convention on host addresses */
#define ADDR_SERVER_M "127.0.0.1"
#define ADDR_SERVER_C "127.0.0.1"
#define ADDR_SERVER_EE "127.0.0.1"
#define ADDR_SERVER_CS "127.0.0.1"

/* Convention on port numbers, my last 3 digits of USC ID is 365 */
#define PORT_SERVER_C "21365"
#define PORT_SERVER_CS "22365"
#define PORT_SERVER_EE "23365"
#define PORT_SERVER_M_UDP "24365"
#define PORT_SERVER_M_TCP "25365"

/* Convention on socket */
#define USE_TCP 1
#define USE_UDP 2
#define BACKLOG 10  // how many pending connections queue will hold for TCP
#define MAXBUFLEN 256  // according to @piazza289, the length of one entry is less than 200 characters

/* Convention on message header type */
#define AUTH_REQUST "ARQ"
#define AUTH_RESULT "ARS"
#define QUERY_REQUEST "QRQ"
#define QUERY_RESULT "QRS"

/* Convention on status codes */
#define SUCCESS 0
#define PASS "PASS"  // authentication request has passed
#define FAIL_NO_USER "FAIL_NO_USER"
#define FAIL_PASS_NO_MATCH "FAIL_PASS_NO_MATCH"
#define FOUND_COURSE_INFO "FOUND_COURSE_INFO"
#define NO_COURSE_CODE "NO_COURSE_CODE"
#define NO_COURSE_INFO "NO_COURSE_INFO"

/* Convention on course structure */
#define COURSE_CODE "course code"
#define CREDIT "Credit"
#define PROF "Professor"
#define DAYS "Days"
#define COURSE_NAME "CourseName"


/* Helper functions */
void parse_key_value_by(char deliminator, std::string origin_data, std::string & key, std::string & value);
std::string encode_msg(std::string header_type, std::string payload);
void decode_msg(std::string encoded_msg, std::string & header, std::string & payload);
void decode_auth_payload(std::string auth_payload, std::string & username, std::string & password);
void parse_course_info(std::string origin_data, std::unordered_map<std::string, std::string> & course_db);
void parse_course_detail(std::string course_info, std::string & credit, std::string & professor, std::string & days, std::string & course_name);

void *get_in_addr(struct sockaddr *sa);
unsigned int get_local_dynamic_port(int sockfd);

int setup_server_socket(int & sockfd, const int socket_type, const std::string port);

/* Sending message via existed TCP socket */
int tcp_send_msg(int sockfd, std::string msg);

/* Receiving message via existed TCP socket */
int tcp_recv_msg(int sockfd, std::string & msg);

/* Sending message via existed UDP socket */
int udp_send_msg(int sockfd, std::string host, std::string port, std::string msg);

/* Receiving message via existed UDP socket
*/
int udp_recv_msg(int sockfd, std::string & msg);
#endif