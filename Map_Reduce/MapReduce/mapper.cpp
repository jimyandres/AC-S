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

std::vector<std::string> getInterval(std::string input, char delim)
{
    std::vector<std::string> inputs;
    std::istringstream f(input);
    std::string tmp;   
    int i =0;
    while (getline(f, tmp, delim) && i < 2) {
        inputs.push_back(tmp);
        i++;
    }
    return inputs;
}

std::string getReducer(json &reducers, std::string key)
{
    std::vector<std::string> interval;
    for (json::iterator it = reducers.begin(); it != reducers.end(); ++it) {
        interval = getInterval(it.key(), '-');
        if(key[0] >= interval.front()[0] && key[0] <= interval.back()[0]) {
            return it.value();
            //std::cout << "Result" << it.value()  << std::endl;
        } else {
            interval.clear();
        }
        //std::cout << it.key() << " : " << it.value() << "\n";
    }
    return "test";
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

    socket s(ctx, socket_type::sub);
    s.connect(master_address);
    s.subscribe("");

    json bootstrapMsg = {
        {"source", "map"},
        {"id", myID}
    };

    bootstrap.send(bootstrapMsg.dump());
    bootstrap.disconnect(bootstrap_address);

    std::string reducers_info;
    json reducers;
    s.receive(reducers_info);
    reducers = json::parse(reducers_info);
    //std::cout << std::setw(4) << reducers << std::endl;
    s.unsubscribe("");
    s.subscribe(myID);

    while(true) {
        std::cout << "Waiting for message to arrive!\n";
        std::string master_req, address;
        s.receive(address);
        s.receive(master_req);
        json message = json::parse(master_req);
        std::cout << "Message received!" << std::endl;
        //std::cout << "Message received! from: [" << address << "]" << std::endl;
        //std::cout << "Data: " << message["data"].get<std::string>() << std::endl;
        std::istringstream data(message["data"].get<std::string>());
        countWords(data);
        json results (words);
        //std::cout << std::setw(4) << results << std::endl;
        
        for (auto& x: words) {
            std::cout << x.first << ": " << x.second << std::endl;
            std::string reducer_address = getReducer(reducers, x.first);
            //std::cout << "Destiny: " << reducer_address << std::endl;
            bootstrap.connect(reducer_address);
            json message = {
                {"word", x.first},
                {"count", x.second}
            };
            bootstrap.send(message.dump());
            bootstrap.disconnect(reducer_address);
        }
        break;
    }
    bootstrap.close();
    s.close();
    ctx.terminate();
    return 0;
}