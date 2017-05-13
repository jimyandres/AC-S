#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <cctype>
#include <zmqpp/zmqpp.hpp>
#include "src/json.hpp"

using namespace zmqpp;
using json = nlohmann::json;

std::unordered_map<std::string,int> words;

std::string getNextToken(std::istream &in)
{
    char c;
    std::string ans="";
    c=in.get();
    while(!std::isalnum(c) && !in.eof()) { //remove non letter charachters
        c=in.get();
    }
    while(std::isalnum(c)) {
        ans.push_back(std::tolower(c));
        c=in.get();
    }
    return ans;
}

void countWords(std::istream &in)
{
    std::string s;
    std::string empty ="";
    while((s=getNextToken(in))!=empty ) {
        ++words[s];
    }
}

void showResults()
{
    for (auto& x: words)
        std::cout << x.first << ": " << x.second << std::endl;
}

int main(int argc, char** argv)
{
    srand(time(NULL));
    if(argc != 3) {
        std::cout << "Usage: ./mapper <bootstrap_address> <master_address>" << std::endl;
        return -1;
    }
    std::string bootstrap_address = "tcp://" + std::string(argv[1]);
    std::string master_address = "tcp://" + std::string(argv[2]);
    //std::string topic = argv[3];
    std::string myID = std::to_string(rand()%100);
    std::cout << "My id: " << myID << std::endl;

	context ctx;
    socket bootstrap(ctx, socket_type::push);
    bootstrap.connect(bootstrap_address);

    bootstrap.send(myID);

    socket s(ctx, socket_type::sub);
    s.subscribe(myID);
    s.connect(master_address);

    while(true) {
        std::cout << "Waiting for message to arrive!\n";
        std::string master_req, address;
        s.receive(address);
        s.receive(master_req);
        json message = json::parse(master_req);

        std::cout << "Message received! from: [" << address << "]" << std::endl;
        std::cout << "Data: " << message["data"].get<std::string>() << std::endl;
        std::istringstream data(message["data"].get<std::string>());
        countWords(data);
        json results (words);
        std::cout << std::setw(4) << results << std::endl;
        break;
    }
    bootstrap.close();
    s.close();
    ctx.terminate();
    return 0;
}