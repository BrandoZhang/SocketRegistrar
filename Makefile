CC = g++
CFLAGS = -g -std=c++11 -Wall

all: client.cpp serverM.cpp serverC.cpp serverCS.cpp serverEE.cpp convention.hpp convention.cpp
	$(CC) $(CFLAGS) -c convention.cpp
	$(CC) $(CFLAGS) -o serverM serverM.cpp convention.o
	$(CC) $(CFLAGS) -o serverC serverC.cpp convention.o
	$(CC) $(CFLAGS) -o serverEE serverEE.cpp convention.o
	$(CC) $(CFLAGS) -o serverCS serverCS.cpp convention.o
	$(CC) $(CFLAGS) -o client client.cpp convention.o

clean:
	rm serverM serverC serverEE serverCS client convention.o -rf *.dSYM/

convention.o: convention.cpp convention.hpp
	$(CC) $(CFLAGS) -c convention.cpp

serverM: serverM.cpp convention.o
	$(CC) $(CFLAGS) -o serverM serverM.cpp convention.o

serverC: serverC.cpp convention.o
	$(CC) $(CFLAGS) -o serverC serverC.cpp convention.o

serverEE: serverEE.cpp convention.o
	$(CC) $(CFLAGS) -o serverEE serverEE.cpp convention.o

serverCS: serverCS.cpp convention.o
	$(CC) $(CFLAGS) -o serverCS serverCS.cpp convention.o

client: client.cpp convention.o
	$(CC) $(CFLAGS) -o client client.cpp convention.o