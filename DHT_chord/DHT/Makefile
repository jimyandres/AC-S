CC	= g++ -std=c++11 -Wall -L$(HOME)/zmq/lib -I$(HOME)/zmq/include
LDFLAGS = -lpthread -lzmqpp -lzmq -lcrypto

all: client node

#server: fileServer.cc
#	$(CC) -o fileServer fileServer.cc $(LDFLAGS)

client: client.cc
	$(CC) -o client client.cc $(LDFLAGS)

node: node.cc
	$(CC) -o node node.cc $(LDFLAGS)

#broker: broker.cc
#	$(CC) -o broker broker.cc $(LDFLAGS)

clean:
	rm -f client node