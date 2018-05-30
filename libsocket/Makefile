all: server client

server: server.o libsocket.o
	g++ server.o libsocket.o -o server

client: client.o libsocket.o
	g++ client.o libsocket.o -o client

server.o: server.cpp
	g++ -c server.cpp

client.o: client.cpp
	g++ -c client.cpp

libsocket.o: libsocket.cpp libsocket.h
	g++ -c libsocket.cpp

clean:
	rm -f *.o server client
