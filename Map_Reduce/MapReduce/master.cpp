#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <cctype>
#include <vector>
#include <chrono>
#include <zmqpp/zmqpp.hpp>
#include "src/json.hpp"

using json = nlohmann::json;
using namespace zmqpp;

void bootstrap_handler(json &req, std::vector<std::string> &mappers, std::unordered_map<std::string, std::string> &reducers)
{
	std::string src = req["source"];
	if(src == "map") {
		mappers.push_back(req["id"]);
	} else if(src == "red") {
		std::string key = req["lower_limit"].get<std::string>() + "-" + req["upper_limit"].get<std::string>();
		std::string reducer = req["address"];
		reducers[key] = reducer;
	} else {
		std::cout << "Unkown source" << std::setw(4) << req << std::endl;
	}
}

int main(int argc, char** argv)
{
	if(argc != 3) {
        std::cout << "Usage: ./word_counter <file_path> <bootstrap_address>" << std::endl;
        return -1;
    }
    std::ifstream fin(argv[1]);
    std::string bootstrap_address = "tcp://" + std::string(argv[2]);

    context ctx;
    socket s(ctx, socket_type::pull);
    s.bind(bootstrap_address);
    
    socket mappers(ctx, socket_type::pub);
    mappers.bind("tcp://*:5555");

    std::string op;
    std::vector<std::string> mappers_ids;
    std::unordered_map<std::string, std::string> reducers;

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
    				json reducers_info (reducers);
    				mappers.send(reducers_info.dump());
		    		break;
		    	} else if(op == "sh") {
		    		std::cout << "Mappers" << std::endl;
		    		for(int i=0; i<(int)mappers_ids.size(); i++) {
		    			std::cout << "Mapper [" << i << "] : " << mappers_ids[i] << std::endl;
		    		}
		    		std::cout << "\nReducers" << std::endl;
		    		for (auto& x: reducers)
				        std::cout << x.first << ": " << x.second << std::endl;
		    	}
    		}
    		if(p.has_input(s)) {
    			std::string message;
		    	s.receive(message);
		    	json req = json::parse(message);
		    	std::cout << "Message received!" << std::endl;
		    	bootstrap_handler(req, mappers_ids, reducers);
				//mappers_ids.push_back(message);
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
    	{"address", bootstrap_address}
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