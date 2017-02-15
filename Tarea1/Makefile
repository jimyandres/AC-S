CC	= g++ -std=c++11 -Wall -L/home/andres/zmq/lib -I/home/andres/zmq/include
LDFLAGS = -lpthread -lzmqpp -lzmq -ljsoncpp 

all: server client

server: fileServer.cc
	$(CC) -o fileServer fileServer.cc $(LDFLAGS)

client: fileClient.cc
	$(CC) -o fileClient fileClient.cc $(LDFLAGS)

clean:
	rm -f fileClient fileServer
