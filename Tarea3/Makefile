CC	= g++ -std=c++11 -Wall -L$(HOME)/zmq/lib -I$(HOME)/zmq/include
LDFLAGS = -lpthread -lzmqpp -lzmq -ljsoncpp -lcrypto

all: server client broker

server: fileServer.cc
	$(CC) -o fileServer fileServer.cc $(LDFLAGS)

client: fileClient.cc
	$(CC) -o fileClient fileClient.cc $(LDFLAGS)

broker: broker.cc
	$(CC) -o broker broker.cc $(LDFLAGS)

clean:
	rm -f fileClient fileServer broker
