# SocketRegistrar

|                Network Topology                 | 
|:-----------------------------------------------:|
| ![Network Topology](/docs/network_topology.png) |

## Project Description

This is a C/C++ socket programming project that simulates a web registration system. 
Concretely, `client` sets up TCP connection with `serverM` to get authorized or query information on a course.
`serverM` acts as a gateway, process the request from `client`, and forward a request via UDP to corresponding backend servers if needed.
`serverC` is a credential server that verified whether a given username-password pair is valid.
`serverEE` and `serverCS` store the information of courses offered
by EE / CS department, respectively.

In this project, I implement Phase 0-4 according to the project specification (exclude the extra part).
What's more, almost all messages exchanged between two ends follows a header-payload format (see `Format of Exchanged Message` part) to distinguish different stages.
Specifically, the `serverM` will generate and store a random token in its memory when a user got authorized, and then send to the `client`.
When `client` is in the stage of query course information, it should contain such a token to prove it is authorized.
Such a design fits the scenario that `client` will new a TCP connection per request and follows the industry paradigm.


## File Structure

```
.
├── Makefile                // Makefile of this project
├── client.cpp              // source code of client, prompts user to login, and query course information
├── convention.cpp          // source code of agreements or utilities shared by communication entities
├── convention.hpp          // header of agreements, e.g., status code, request/response header
├── cred.txt                // mocked database file, containing encrypted usernames and passwords, only be accessible by the serverC 
├── cred_unencrypted.txt    // unencrypted version of `cred.txt`, only for testing 
├── cs.txt                  // mocked database file, containing CS course information, only be accessible by the serverCS
├── ee.txt                  // mocked database file, containing EE course information, only be accessible by the serverEE
├── readme.md               // README of this project
├── serverC.cpp             // source code of serverC, parses `cred.txt` and stores username-password pairs in a hashmap 
├── serverCS.cpp            // source code of serverCS, parses `cs.txt` and stores coursecode-info pairs in a hashmap
├── serverEE.cpp            // source code of serverEE, parses `ee.txt` and stores coursecode-info pairs in a hashmap
└── serverM.cpp             // source code of serverM, processes request from `client` and makes reponses after contacting other backend servers
```

## How to Use

1. Set up input files

    `serverC`, `serverCS`, and `serverEE` require to read files to function properly.
    By default, these files are hardcoded in their source code in the format like `const string COURSE_FILE = "cs.txt";`.
    You can change if needed.

2. Compile

    In Ubuntu, compile with `make all`.

3. Start

   Run in the following sequence (but in 5 different terminals):
   ```
   ./serverM
   ./ServerC
   ./serverEE
   ./serverCS
   ./client
   ```


## Format of Exchanged Message

Basically, each exchanged message will contain a `header` and a `payload`, separated by a single space.
Since all the headers are added and parsed by the programs, the `payload`, which may come from user input, will not be broken.

1. Authentication request (from `client` to `serverM` / from `serverM` to `serverC`)

    ["ARQ" "<number_of_username_chars>" "<username><password>"]

    "ARQ" is a prefix served as a header for `serverM` or `serverC` to recognize the message type.
    "<number_of_username_chars>" is the number of chars of the username typed by the user, this design is to fit the scenario when `username` contains a possible delimiter used by the server.
    "<username>" and "<password>" are what user typed, with `\r\n` removed.

2. Authentication result 

   2.1 From `serverC` to `serverM`: 

    ["ARS" "<result_code>" "<dummy_info>"]

    "ARS" is a prefix served as a header for `serverM` to recognize the message type.
    "<result_code>" is one of `PASS`, `FAIL_NO_USER`, and `FAIL_PASS_NO_MATCH` as defined in `convention.hpp`.
    "<dummy_info>" is just appended for format consistency, and will be omitted by `serverM`.

   2.2 From `serverM` to `client`: 
   
    ["ARS" "<result_code>" "<token_or_dummy_info>"]

    "ARS" is a prefix served as a header for `client` to recognize the message type.
    "<result_code>" is one of `PASS`, `FAIL_NO_USER`, and `FAIL_PASS_NO_MATCH` as defined in `convention.hpp`.
    "<token_or_dummy_info>" will be the `serverM` generated token if user got authorized, otherwise, it will be dummy info.

3. Query request

   3.1 From `client` to `serverM`:

   ["QRQ" "<token>" "<course_code>" "<category>"]
   "QRQ" is a prefix served as a header for `serverM` to recognize the message type.
   "<token>" is what `serverM` has sent to `client` when authorized, it served as an identifier.
   "<course_code>" and "<category>" are what user typed.

   3.2 From `serverM` to `serverCS` or `serverEE`:

   ["QRQ" "<course_code>" "<category>"]
   "ARQ" is a prefix served as a header for `serverCS` or `serverEE` to recognize the message type.
   "<course_code>" and "<category>" are what user typed.

4. Query result (from `serverCS` or `serverEE` to `serverM` / from `serverM` to `client`)

   ["QRS" "<result_code>" "<info>"]
   "QRS" is a prefix served as a header for `serverM` or `client` to recognize the message type.
   "<result_code>" is one of `FOUND_COURSE_INFO` or `NO_COURSE_CODE` as defined in `convention.hpp`.
   "<info>" is the info the `client` is looking for if the request is valid, otherwise, it will be dummy info.


## Reference

[Beej's Guide to Network Programming](https://beej.us/guide/bgnet/html/)

[Remove tailing `\r` and `\n` from string](https://stackoverflow.com/questions/45956271/stdgetline-reads-carriage-return-r-into-the-string-how-to-avoid-that)

[Get local dynamic port number](https://piazza.com/class/l7dlij7ko7v4bv/post/188)

[Generate random strings in C++](https://stackoverflow.com/a/12468109)

[Print out key-value pairs in unordered_map](https://www.techiedelight.com/print-keys-values-map-cpp/)