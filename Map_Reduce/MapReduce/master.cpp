#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <vector>
#include <chrono>
#include <zmqpp/zmqpp.hpp>
#include "src/json.hpp"

using json = nlohmann::json;
using namespace zmqpp;

int main(int argc, char** argv)
{
	if(argc != 3) {
        std::cout << "Usage: ./word_counter <file_path> <address>" << std::endl;
        return -1;
    }
    std::ifstream fin(argv[1]);
    std::string address = "tcp://" + std::string(argv[2]);

    context ctx;
    socket s(ctx, socket_type::pull);
    s.bind(address);
    
    socket mappers(ctx, socket_type::pub);
    mappers.bind("tcp://*:5555");

    std::string op;
    std::vector<std::string> mappers_ids;

    int standardin = fileno(stdin);
	poller p;

	p.add(standardin, poller::poll_in);
	p.add(s, poller::poll_in);

	//Register all the mappers that will be available
    while(true) {
    	std::cout << "Waiting for message to arrive!\n";
    	if(p.poll()) {
    		if(p.has_input(standardin)) {
    			std::cin >> op;
    			if(op == "ready") {
		    		break;
		    	} else if(op == "sh") {
		    		for(int i=0; i<(int)mappers_ids.size(); i++) {
		    			std::cout << "Mapper [" << i << "] : " << mappers_ids[i] << std::endl;
		    		}
		    	}
    		}
    		if(p.has_input(s)) {
    			std::string message;
		    	s.receive(message);
		    	std::cout << "Message received! " << message << std::endl;
				mappers_ids.push_back(message);
    		}
    	}
    }

    /*Send parts of the file to mappers*/
    int nmappers = mappers_ids.size();
    fin.seekg(0, fin.end);
    int size = fin.tellg();
    fin.seekg(0, fin.beg);

	int begin = 0;
    int batch_const = size/nmappers;
    json message = {
    	{"address", address}
    };
    for(int i=0; i<nmappers; i++) {
        int batch_size;
        if(i == nmappers - 1)
            batch_size = (size - begin);
        else
            batch_size = (batch_const * (i+1)) - begin;
        int end = begin + batch_size;
        if(end > size)
            end = size;
        fin.seekg(batch_size, fin.cur);
        char c;
        if(fin.tellg() != size) {
            c=fin.peek();
            while(!fin.eof() && !isspace(c)) {
                c=fin.get();
                end++;
                c=fin.peek();
            }
        }
        fin.seekg(-(end-begin), fin.cur);
        char * data = new char [(end - begin)];
        fin.read(data, (end - begin));
        std::string tmp(data, (end-begin));
        message["data"] = tmp;
        mappers.send(mappers_ids[i], socket::send_more);
        mappers.send(message.dump());
        data = NULL;
        //std::cout << std::setw(4) << message << std::endl;
        //std::cout << message["data"].get<std::string>() << std::endl;
        delete[] data;
        begin = end;
    }

    /*here should receive the results from the reducers*/
    while(true) {
    	if(p.poll()) {
    		if(p.has_input(standardin)) {
    			std::cin >> op;
    			if(op == "ex") {
		    		break;
		    	}
		    }
    	}
    }
    s.close();
    mappers.close();
	ctx.terminate();
    return 0;
}