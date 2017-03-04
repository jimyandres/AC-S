CC	= g++ -std=c++11 -Wall -L$(HOME)/zmq/lib -I$(HOME)/zmq/include
LDFLAGS = -lpthread -lzmqpp -lzmq -lcrypto

all: broker server client

server: fileServer.cc
	$(CC) -o fileServer fileServer.cc $(LDFLAGS)

client: fileClient.cc
	$(CC) -o fileClient fileClient.cc $(LDFLAGS)

broker: broker.cc
	$(CC) -o broker broker.cc $(LDFLAGS)

clean:
	rm -f broker fileClient fileServer
