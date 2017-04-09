#include <iostream>
#include <sstream>
//#include <stdio.h>
#include <string.h>
#include <vector>
#include <zmqpp/zmqpp.hpp>
#include <openssl/sha.h>
#include "src/json.hpp"

using namespace std;
using namespace zmqpp;
using json = nlohmann::json;

void ChecksumToString(unsigned char * check_sum, char mdString[SHA_DIGEST_LENGTH*2+1]) {
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)check_sum[i]);
}

void getID(string Value, char ans[SHA_DIGEST_LENGTH*2+1]) {
	unsigned char check_sum[SHA_DIGEST_LENGTH];
	
	SHA1((unsigned char *)Value.c_str(), Value.size(), (unsigned char *)&check_sum);
	ChecksumToString((unsigned char *)&check_sum, ans);
}

vector<string> getOptions(string input) {
	vector<string> inputs;
    istringstream f(input);
    string tmp;   
    int i =0;
    while (getline(f, tmp, ' ') && i < 2) {
        //cout << tmp << endl;
        inputs.push_back(tmp);
        i++;
    }
    return inputs;
}

int main(int argc, char** argv) {
	if(argc != 3) {
		cout << "Enter server address and client address!" << endl;
		return EXIT_FAILURE;
	}

	// Socket to request to servers
	context ctx;
	socket req_serverSocket(ctx, socket_type::push);	
	req_serverSocket.connect("tcp://localhost:" + string(argv[1]));

	// Socket to receive servers answers
	string client_address = "tcp://*:" + string(argv[2]);
	string nodeName = "tcp://localhost:" + string(argv[2]);
	cout << nodeName << endl;
	socket ans_serverSocket(ctx, socket_type::pull);	
	ans_serverSocket.bind(client_address);

	int standardin = fileno(stdin);
	poller p;

	p.add(standardin, poller::poll_in);
	p.add(ans_serverSocket, poller::poll_in);

	while(true) {
		cout << "Enter something to send: " << endl;
		if(p.poll()) {
			if(p.has_input(standardin)) {
				string val;
				vector<string> inputs;
				char key[SHA_DIGEST_LENGTH*2+1];
				getline(cin, val);
				if(val != "") {
					inputs = getOptions(val);
					if(inputs.front() == "q" || inputs.front() == "quit" || inputs.front() == "Quit" || inputs.front() == "ex" || inputs.front() == "Exit" || inputs.front() == "exit") {
						break;
					}
					if(inputs.size() < 2) {
						cout << "Missing argument!" << endl;
					} else {
						cout << "Input is: " << inputs.back() << endl;
						//cout << "Input is: " << val << endl;
						getID(inputs.back(), key);

						//message req;
						json req = {
							{"source", "client"},
							{"id", nodeName},
							{"address", nodeName},
							{"op", inputs.front()},
							{"key", key},
							{"val", inputs.back()}
						};
						//req << nodeName << nodeName << key << inputs.front() << inputs.back();
						cout << setw(4) << req << endl;
						cout << req.dump() << endl;
						req_serverSocket.send(req.dump());
						cout << "Message sended" << endl;
					}
				}
			}
			if(p.has_input(ans_serverSocket)) {
				//string status, ok_msg, err_msg;
				string res;
				ans_serverSocket.receive(res);
				json ans = json::parse(res);

				//res >> status;
				if(ans["status"] == "Ok") {
					//res >> ok_msg;
					cout << ans["msg"] << endl;
				} else if(ans["status"] == "Error") {
					//res >> err_msg;
					cout << ans["msg"] << endl;
				}
			}
		}
	}

	req_serverSocket.close();
	ans_serverSocket.close();
	ctx.terminate();
	return 0;
}