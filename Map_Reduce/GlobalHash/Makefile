CC	= g++ -std=c++11 -Wall
LDFLAGS = -lpthread

all: counter

counter: word_counter.cpp
	$(CC) -o word_counter word_counter.cpp $(LDFLAGS)

clean:
	rm -f word_counter
