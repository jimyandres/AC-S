#include <iostream>
#include <string>
#include <fstream>
#include <zmqpp/zmqpp.hpp>
#include "src/json.hpp"
#include <unordered_map>
#include <cctype>
#include <thread>
#include <mutex>
#include <vector>
#include <sstream>

using namespace zmqpp;
using json = nlohmann::json;

static std::vector<string> workers;

void requestHandler();

int main(int argc, char** argv){
//    string myAddress = "tcp://";

    if(argc != 3) {
        std::cout << "Usage: ./word_counter <file_path> <my_address:port>" << std::endl;
        return -1;
    }

    std::ifstream fin(argv[1]);
    myAddress.append(argv[2]);

    context ctx;
    socket mappers_sock(ctx, socket_type::pair);
    socket reducers_sock(ctx, socket_type::pair);

    std::cout << "Binding socket to tcp port 5555 for mappers and tcp port 5556 for reducers\n";
    mappers_sock.bind("tcp://*:5555");
    reducers_sock.bind("tcp://*:5556");

    int standardin = fileno(stdin);
    poller p;
    p.add(mappers_sock, poller::poll_in);
    p.add(reducers_sock, poller::poll_in);
    p.add(standardin, poller::poll_in);

    while(true) {
        std::cout << "Waiting for Mappers to arrive!\n";
        if(p.poll()) {
            if(p.has_input(mappers_sock) || p.has_input(reducers_sock)) {
                string request;
                mappers_sock.receive(request);
                std::cout << "Message received!\n";
                json req = json::parse(request);
                requestHandler(req);
            }
            if(p.has_input(standardin)) {
                string input;
                getline(cin, input);
                if(input == "q" || input == "quit" || input == "Quit" || input == "ex" || input == "Exit" || input == "exit") {
                    std::cout << "Total mappers: " << workers.size() << std::endl;
                    break;
                }
//                else if(input == "sh") {
//                    HeapSort(servers_queue);
//                }
            }
        }
    }



    // parts will store
    json parts;

}


