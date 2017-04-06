#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <zmqpp/zmqpp.hpp>
#include <openssl/sha.h>
#include <unordered_map>

using namespace std;
using namespace zmqpp;

static unordered_map<string, socket*> clients;
static context ctx;
static unordered_map<string, string> hashTable;
static string lowerBound = "", upperBound = "";

void ChecksumToString(unsigned char * check_sum, char mdString[SHA_DIGEST_LENGTH*2+1]) {
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)check_sum[i]);
}

string interval() {
	return "(" + lowerBound + "," + upperBound + "]";
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

void localOP(message &req, string key, string idClient) {
	string op, val;
	req >> op;
	message response;
	if(op == "insert") {
		req >> val;
		hashTable.emplace(key,val);
		response << "Ok" << "Val: \"" + val + "\" with key: \"" + key + "\" successfully inserted at: \"" + upperBound + "\""; 
		cout << "Stored Key: " << key << " Val: " << val << " at: " << upperBound << endl;
	} else if(op == "delete") {
		auto it = hashTable.find(key);
		if(it != hashTable.end()){
			val = hashTable[key];
			hashTable.erase(key);
			response << "Ok" << "Val: \"" + val + "\" with key: \"" + key + "\" successfully deleted from: \"" + upperBound + "\"";
			cout << "Key: " << key << " with Val: " << val << " deleted from: " << upperBound << endl;
		} else {
			response << "Error: " << " Key: " + key + " not found";
			cout << "Key: " << key << " not found!!" << endl;
		}
	} else if(op == "search") {
		auto it = hashTable.find(key);
		if(it != hashTable.end()){
			val = hashTable[key];
			response << "Ok" << "Key: \"" + key + "\" is associated with the value: \"" + val + "\" at: \"" + upperBound + "\"" ;
			cout << "The Key: " << key << " is associated with the value: " << val << " at: " << upperBound << endl;
		} else {
			response << "Error: " << " Key: " + key + " not found";
			cout << "Key: " << key << " not found!!" << endl;
		}
	} else if(op == "AddNode") {
		message delegate_req;
		delegate_req.copy(req);
		string nodeId, nodeAddress;
		
		req >> nodeId >> nodeAddress;
		if(inRange(nodeId)) {
			cout << "I have your keys" << endl;
		} else {
			cout << "Error: Not in my range, delegating..." << endl;
			successor.send(delegate_req);
		}


		/*auto it = hashTable.find(key);
		if(it != hashTable.end()){
			val = hashTable[key];
			response << "Ok" << "Key: \"" + key + "\" is associated with the value: \"" + val + "\" at: \"" + upperBound + "\"" ;
			cout << "The Key: " << key << " is associated with the value: " << val << " at: " << upperBound << endl;
		} else {
			response << "Error: " << " Key: " + key + " not found";
			cout << "Key: " << key << " not found!!" << endl;
		}*/
	} else {
		response << "Error: " << " invalid option: " + op;
		cout << "Unknown option: " << op << endl;
	}
	clients[idClient]->send(response);
}

void handleClientRequest(message &req, socket &successor) {
	message delegate_req;
	delegate_req.copy(req);
	
	string idClient, addressClient, key;

	if(req.get(0) == "AddNode" || req.get(0) == "DeleteNode") {
		localOP(req, '', '');
	}
	req >> idClient >> addressClient;
	
	auto it = clients.find(idClient);
	if(it == clients.end()) {
		socket* sc = new socket(ctx, socket_type::push);
		clients.emplace(idClient,sc);
		clients[idClient]->connect(addressClient);
	}
	
	req >> key;
	if(inRange(key)) {
		cout << "Key " << key << " is mine!" << endl;
		localOP(req, key, idClient);
	} else {
		cout << "Error: Not my responsability, delegating..." << endl;
		successor.send(delegate_req);
	}
}

void deleteSockets() {
	for(auto& e:clients) {
		delete e.second;
	}
}

int main(int argc, char** argv) {
	srand(time(NULL));
	if(argc != 5) {
		cout << "Enter myAddress successorAddress clientsAddress" << endl;
		return EXIT_FAILURE;
	}
	char myID[SHA_DIGEST_LENGTH*2+1];
	string myAddress, successorAddress, clientsAddress, op;

	int bootstrap = atoi(argv[1]);
	myAddress = argv[2];
	successorAddress = argv[3];
	clientsAddress = argv[4];

	cout << "Listening on " << myAddress << " and connectig to neighbor on " << successorAddress << endl;

	socket mySocket(ctx, socket_type::pair);
	mySocket.bind("tcp://*:" + string(myAddress));
	string address = "tcp://localhost:" + string(myAddress);

	socket mySuccessor(ctx, socket_type::pair);
    mySuccessor.connect("tcp://localhost:" + string(successorAddress));

    getID(to_string(rand()%100), myID);
    upperBound = string(myID);

    if(bootstrap) {
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
		cout << "Responsible for keys in " << interval() << endl;
    } else {
    	cout << "Connect to existing chord, not implemented yet!" << endl;
    	/*****************************************************************
		send my id
		send my address
		******************************************************************/

		message id_msg, id_msg_res;
		id_msg << "AddNode" << myID << address;
		mySuccessor.send(id_msg);

	    mySocket.receive(id_msg_res);
    }

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
					cout << "Clients Table contains: " << endl;
					for (auto& x: clients)
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
	deleteSockets();
	mySocket.close();
	mySuccessor.close();
	clientsSocket.close();
	ctx.terminate();
	return 0;
}
