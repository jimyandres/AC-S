#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <zmqpp/zmqpp.hpp>
#include <openssl/sha.h>
#include <unordered_map>
#include "src/json.hpp"

using namespace std;
using namespace zmqpp;
using json = nlohmann::json;

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

void localOP(json &req) {
	string op, key, val;
	op = req["op"];
	key = req["key"];
	json response;
	if(op == "insert") {
		val = req["val"];
		hashTable.emplace(key,val);
		response = {
			{"status", "Ok"},
			{"msg", "Val: '" + val + "' with key: '" + key + "' successfully inserted at: '" + upperBound + "'"}
		};
		cout << "Stored Key: " << key << " Val: " << val << " at: " << upperBound << endl;
	} else if(op == "delete") {
		auto it = hashTable.find(key);
		if(it != hashTable.end()){
			val = hashTable[key];
			hashTable.erase(key);
			response = {
				{"status", "Ok"},
				{"msg", "Val: '" + val + "' with key: '" + key + "' successfully deleted from: '" + upperBound + "'"}
			};
			cout << "Key: " << key << " with Val: " << val << " deleted from: " << upperBound << endl;
		} else {
			response = {
				{"status", "Error"},
				{"msg", "Key: '" + key + "' not found"}
			};
			cout << "Key: " << key << " not found!!" << endl;
		}
	} else if(op == "search") {
		auto it = hashTable.find(key);
		if(it != hashTable.end()){
			val = hashTable[key];
			response = {
				{"status", "Ok"},
				{"msg", "Key: '" + key + "' is associated with the value: '" + val + "' at: '" + upperBound + "'"}
			};
			cout << "The Key: " << key << " is associated with the value: " << val << " at: " << upperBound << endl;
		} else {
			response = {
				{"status", "Error"},
				{"msg", "Key: '" + key + "' not found"}
			};
			cout << "Key: " << key << " not found!!" << endl;
		}
	} else {
		response = {
			{"status", "Error"},
			{"msg", "Invalid option: " + op}
		};
		cout << "Unknown option: " << op << endl;
	}
	clients[req["id"]]->send(response.dump());
}

void nodeOps(json &req, socket &successor) {
	string op, id;
	op = req["op"];
	id = req["data"];
	json response;
	
	if(op == "AddNode") {
		if(inRange(id)) {
			cout << "I have your Keys" << endl;
		} else {
			cout << "Error: Not in my range, delegating..." << endl;
			successor.send(req.dump());
		}
	} else if(op =="DeleteNode") {
		cout << "Taking over your keys" << endl;
	} else if(op =="UpdateSucc") {
		cout << "My successor changed" << endl;
	} else {
		response = {
			{"status", "Error"},
			{"msg", "Invalid option: " + op}
		};
		cout << "Unknown option: " << op << endl;
	}
}

void handleClientRequest(json &req, socket &successor) {
	if(req["source"] == "node") {
		nodeOps(req, successor);
	} else if(req["source"] == "client") {
		string idClient, addressClient, key;
		idClient = req["id"];
		addressClient = req["address"];

		auto it = clients.find(idClient);
		if(it == clients.end()) {
			socket* sc = new socket(ctx, socket_type::push);
			clients.emplace(idClient,sc);
			clients[idClient]->connect(addressClient);
		}

		key = req["key"];
		if(inRange(key)) {
			cout << "Key " << key << " is mine!" << endl;
			localOP(req);
		} else {
			cout << "Error: Not my responsability, delegating..." << endl;
			successor.send(req.dump());
		}
	} else {
		cout << "Unkown source, ignoring message!!" << endl;
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
    	string id_msg_res;
    	json ans;
    	json id_msg = {
    		{"op", "send_id"},
    		{"data", myID}
    	};
	    mySuccessor.send(id_msg.dump());

	    mySocket.receive(id_msg_res);
	    ans = json::parse(id_msg_res);
		if(ans["op"] == "send_id") {
			lowerBound = ans["data"];
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
		string res;
		json ans;
    	json id_msg = {
    		{"source", "node"},
    		{"op", "AddNode"},
    		{"data", myID},
    		{"address", address}
    	};

    	socket tmp(ctx, socket_type::push);
    	tmp.connect("tcp://localhost:" + string(successorAddress));
		tmp.send(id_msg.dump());
		tmp.close();

	    mySocket.receive(res);
	    ans = json::parse(res);

	    /*lower_bound, mysuccesor*/
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
				string node_req;
				mySocket.receive(node_req);
				json req = json::parse(node_req);
				handleClientRequest(req, mySuccessor);
			}
			if(p.has_input(clientsSocket)) {
				cout << "input from client" << endl;
				string client_req;
				clientsSocket.receive(client_req);
				json req = json::parse(client_req);
				handleClientRequest(req, mySuccessor);
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
