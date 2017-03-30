#include <iostream>
#include <stdlib.h>
#include <time.h>
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

bool inRange(string lowerBound, string upperBound, string key) {
	if (upperBound >= lowerBound) {
		return key > upperBound and key <= upperBound;
	} else {
		return key >= upperBound or key < upperBound;
	}
}

int main(int argc, char** argv) {
	srand(time(NULL));
	if(argc != 4) {
		cout << "Enter myAddress successorAddress clientsAddress" << endl;
		return EXIT_FAILURE;
	}
	char myID[SHA_DIGEST_LENGTH*2+1];
	string lowerBound = "", upperBound = "", op;
	string myAddress, successorAddress, clientsAddress;
	myAddress = argv[1];
	successorAddress = argv[2];
	clientsAddress = argv[3];

	cout << "Listening on " << myAddress << " and connectig to neighbor on " << successorAddress << endl;

	context ctx;
	socket mySocket(ctx, socket_type::pair);
	mySocket.bind("tcp://*:" + string(myAddress));

	socket mySuccessor(ctx, socket_type::pair);
    mySuccessor.connect("tcp://localhost:" + string(successorAddress));

    socket clientsSocket(ctx, socket_type::rep);
	clientsSocket.bind("tcp://*:" + string(clientsAddress));

    getID(to_string(rand()%100), myID);
    upperBound = string(myID);

    message id_msg, id_msg_res;
    id_msg << "send_id" << myID;
    mySuccessor.send(id_msg);

    mySocket.receive(id_msg_res);
    id_msg_res >> op;
	if(op == "send_id") {
		id_msg_res >> lowerBound;
	} else {
		cout << "Unknown option: " << op << endl;
	}
	cout << "(" << lowerBound << "," << upperBound << "]" << endl;
    
	cout << "Listening to clients on port " << clientsAddress << endl;
	while(true) {
		message client_req, server_rep;
		string op, key, val;
		clientsSocket.receive(client_req);
		client_req >> op;
		if(op == "Post") {
			client_req >> key >> val;
			cout << "Key: " << key << " Val: " << val << " Responsable?: " << inRange(lowerBound, upperBound, key) << endl;

			if(inRange(lowerBound, upperBound, key)) {
				server_rep << "Ok" << key;
			} else {
				server_rep << "Error" << "Not my responsability";
			}
			clientsSocket.send(server_rep);
		} else {
			cout << "Unknown option: " << op << endl;
		}
	}
	mySocket.close();
	mySuccessor.close();
	clientsSocket.close();
	ctx.terminate();
	return 0;
}