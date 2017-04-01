#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <zmqpp/zmqpp.hpp>
#include <openssl/sha.h>
#include <unordered_map>

using namespace std;
using namespace zmqpp;

unordered_map<string, socket> clients;
context ctx;
unordered_map<string, string> hashTable;
string lowerBound = "", upperBound = "";

void ChecksumToString(unsigned char * check_sum, char mdString[SHA_DIGEST_LENGTH*2+1]) {
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)check_sum[i]);
}

void getID(string Value, char ans[SHA_DIGEST_LENGTH*2+1]) {
	unsigned char check_sum[SHA_DIGEST_LENGTH];
	
	SHA1((unsigned char *)Value.c_str(), Value.size(), (unsigned char *)&check_sum);
	ChecksumToString((unsigned char *)&check_sum, ans);
}

bool inRange(string key) {
	if (upperBound >= lowerBound) {
		return key > upperBound and key <= upperBound;
	} else {
		return key >= upperBound or key < upperBound;
	}
}

void localOP(message &req, string key) {
	string op, val;
	req >> op;
	if(op == "insert") {
		req >> val;
		//hashTable[key] = val;
		hashTable.emplace(key,val);
		cout << "Stored Key: " << key << " Val: " << val << " at: " << upperBound << endl;
	} else if(op == "delete") {
		val = hashTable[key];
		hashTable.erase(key);
		cout << "Key: " << key << " with Val: " << val << " deleted from: " << upperBound << endl;
	} else if(op == "search") {
		val = hashTable[key];
		cout << "The Key: " << key << " is associated with the value: " << val << " at: " << upperBound << endl;
	} else {
		cout << "Unknown option: " << op << endl;
	}
}

void handleClientRequest(message &req, socket &successor) {
	message delegate_req;
	delegate_req.copy(req);
	
	string idClient, addressClient, key;
	req >> idClient >> addressClient;
	
	socket sc(ctx, socket_type::push);
	sc.connect(addressClient);
	//clients[idClient] = sc;
	//pair<string, socket>client(idClient, sc);
	//clients.insert(client);
	
	req >> key;
	if(inRange(key)) {
		cout << "Key " << key << " is mine!" << endl;
		localOP(req, key);
	} else {
		cout << "Error: Not my responsability, delegating..." << endl;
		successor.send(delegate_req);
	}
}

int main(int argc, char** argv) {
	srand(time(NULL));
	if(argc != 4) {
		cout << "Enter myAddress successorAddress clientsAddress" << endl;
		return EXIT_FAILURE;
	}
	char myID[SHA_DIGEST_LENGTH*2+1];
	string myAddress, successorAddress, clientsAddress, op;

	myAddress = argv[1];
	successorAddress = argv[2];
	clientsAddress = argv[3];

	cout << "Listening on " << myAddress << " and connectig to neighbor on " << successorAddress << endl;

	//context ctx;
	socket mySocket(ctx, socket_type::pair);
	mySocket.bind("tcp://*:" + string(myAddress));

	socket mySuccessor(ctx, socket_type::pair);
    mySuccessor.connect("tcp://localhost:" + string(successorAddress));

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
		return EXIT_FAILURE;
	}
	cout << "Responsible for keys in (" << lowerBound << "," << upperBound << "]" << endl;

	socket clientsSocket(ctx, socket_type::pull);
	clientsSocket.bind("tcp://*:" + string(clientsAddress));
	cout << "Listening to clients on port " << clientsAddress << endl;

	int standardin = fileno(stdin);
	poller p;

	p.add(standardin, poller::poll_in);
	p.add(mySocket, poller::poll_in);
	p.add(clientsSocket, poller::poll_in);

	while(true) {
		if(p.poll()) {
			if(p.has_input(standardin)) {
				string input;
				getline(cin, input);
				if(input == "q" || input == "quit" || input == "Quit" || input == "ex" || input == "Exit" || input == "exit") {
					break;
				} else if(input == "sh") {
					cout << "Hash Table contains: " << endl;
					for (auto& x: hashTable)
						cout << x.first << ": " << x.second << endl;
				}
			}
			if(p.has_input(mySocket)) {
				cout << "input from node" << endl;
				message node_req;
				mySocket.receive(node_req);
				handleClientRequest(node_req, mySuccessor);
			}
			if(p.has_input(clientsSocket)) {
				cout << "input from client" << endl;
				message client_req;
				clientsSocket.receive(client_req);
				handleClientRequest(client_req, mySuccessor);
			}
		}
	}

	mySocket.close();
	mySuccessor.close();
	clientsSocket.close();
	ctx.terminate();
	return 0;
}