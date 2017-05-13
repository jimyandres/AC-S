CC	= g++ -std=c++11 -Wall -L$(HOME)/zmq/lib -I$(HOME)/zmq/include
LDFLAGS = -lpthread -lzmqpp -lzmq

all: master mapper

master: master.cpp
	$(CC) -o master master.cpp $(LDFLAGS)

mapper: mapper.cpp
	$(CC) -o mapper mapper.cpp $(LDFLAGS)

clean:
	rm -f master mapper
