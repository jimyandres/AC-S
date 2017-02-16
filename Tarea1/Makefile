<<<<<<< HEAD
CC	= g++ -std=c++11 -Wall -L/home/johan/zmq/lib -I/home/johan/zmq/include
LDFLAGS = -lpthread -lzmqpp -lzmq -ljsoncpp -lcrypto
=======
CC	= g++ -std=c++11 -Wall -L$(HOME)/zmq/lib -I$(HOME)/zmq/include
LDFLAGS = -lpthread -lzmqpp -lzmq -ljsoncpp 
>>>>>>> v2

all: server client

server: fileServer.cc
	$(CC) -o fileServer fileServer.cc $(LDFLAGS)

client: fileClient.cc
	$(CC) -o fileClient fileClient.cc $(LDFLAGS)

clean:
	rm -f fileClient fileServer
