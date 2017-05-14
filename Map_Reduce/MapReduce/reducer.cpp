#include <iostream>
#include <unordered_map>
#include <string>
#include <zmqpp/zmqpp.hpp>
#include "src/json.hpp"

using namespace zmqpp;
using json = nlohmann::json;

int main(int argc, char** argv)
{
	if(argc != 5) {
		std::cout << "./reducer <bootstrap_address> <my_address> <lower_limit> <upper_limit>" << std::endl;
		return -1;
	}
	std::unordered_map<std::string,int> words;
	//json words;
	std::string bootstrap_address = "tcp://" + std::string(argv[1]);
	std::string address = "tcp://" + std::string(argv[2]);
	std::string lower_limit = argv[3];
	std::string upper_limit = argv[4];
	context ctx;
	socket bootstrap(ctx, socket_type::push);
	bootstrap.connect(bootstrap_address);

	socket s(ctx, socket_type::pull);
	s.bind(address);

	json bootstrapMsg = {
		{"source", "red"},
		{"address", address},
		{"lower_limit", lower_limit},
		{"upper_limit", upper_limit}
	};
	std::cout << std::setw(4) << bootstrapMsg << std::endl;

	bootstrap.send(bootstrapMsg.dump());

	int standardin = fileno(stdin);
	poller p;

	p.add(standardin, poller::poll_in);
	p.add(s, poller::poll_in);

	//std::cout << "address: " << address << "\ninterval: " << lower_limit << "-" << upper_limit << std::endl;
	while(true) {
		//std::cout << "Waiting for message to arrive!\n";
		if(p.poll(5000)) {
			if(p.has_input(standardin)) {
				std::string op;
    			std::cin >> op;
    			if(op == "ex") {
    				break;
    			} else if(op == "sh") {
		    		std::cout << "Words" << std::endl;
		    		// for (json::iterator it = words.begin(); it != words.end(); ++it) {
		    		//     std::cout << it.key() << " : " << it.value() << "\n";
		    		// }
		    		for (auto& x: words)
				        std::cout << x.first << ": " << x.second << std::endl;
		    	}
    		}
    		if(p.has_input(s)) {
    			std::string message;
				json data;
				s.receive(message);
				data = json::parse(message);
				std::string word = data["word"];
				int count = data["count"];

				auto it = words.find(word);
				if(it != words.end()) {
					words[word] += count;
				} else {
					words[word] = count;
				}
    		}
		} else {
			json result (words);
			bootstrap.send(result.dump());
			break;
		}
		//std::cout << std::setw(4) << data << std::endl;
	}
	bootstrap.close();
	s.close();
	ctx.terminate();
	return 0;
}