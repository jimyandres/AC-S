#include <iostream>
//#include <stdio.h>
#include <string.h>
#include <zmqpp/zmqpp.hpp>
#include <openssl/sha.h>

using namespace std;
using namespace zmqpp;

void ChecksumToString(unsigned char * check_sum, char mdString[SHA_DIGEST_LENGTH*2+1]) {
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)check_sum[i]);
}

void getID(string Value, char ans[SHA_DIGEST_LENGTH*2+1]) {
	unsigned char check_sum[SHA_DIGEST_LENGTH];
	
	SHA1((unsigned char *)Value.c_str(), Value.size(), (unsigned char *)&check_sum);
	ChecksumToString((unsigned char *)&check_sum, ans);
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

	cout << "Enter something to send: " << endl;
	while(true) {
		if(p.poll()) {
			if(p.has_input(standardin)) {
				string val;
				char key[SHA_DIGEST_LENGTH*2+1];
				getline(cin, val);
				if(val == "q" || val == "quit" || val == "Quit" || val == "ex" || val == "Exit" || val == "exit") {
					break;
				}
				cout << "Input is: " << val << endl;
				getID(val, key);

				message req;
				req << nodeName << nodeName << key << "insert" << val;
				req_serverSocket.send(req);
				cout << "Message sended" << endl;
			}
			if(p.has_input(ans_serverSocket)) {
				string status, hash, err_msg;
				message res;
				ans_serverSocket.receive(res);

				res >> status;
				if(status == "Ok") {
					res >> hash;
					cout << "Hash: " << hash << endl;
				} else if(status == "Error") {
					res >> err_msg;
					cout << "Error: " << err_msg << endl;
				}
				//cout << key << req << endl;
			}
		}
	}

	req_serverSocket.close();
	ans_serverSocket.close();
	ctx.terminate();
	return 0;
}