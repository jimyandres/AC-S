#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <vector>
#include <zmqpp/zmqpp.hpp>
#include <openssl/sha.h>
#include <unordered_map>
#include "src/json.hpp"

using namespace std;
using namespace zmqpp;
using json = nlohmann::json;

static unordered_map<string, socket*> clients;
static unordered_map<string, socket *> connections;
static context ctx;
static poller p;
static unordered_map<string, string> hashTable;
static string lowerBound = "", upperBound = "", myAddress = "tcp://", neighborAddress = "tcp://";

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
		return (key > lowerBound && key <= upperBound);
	} else {
		return (key > lowerBound || key <= upperBound);
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

void sendToNode() {
	if (hashTable.empty()) {
		cout << "Nothing to send..." << endl;
	} else {
		vector<string> tmp;
		for (auto& x: hashTable) {
			if(!inRange(x.first)) {
				cout << x.first << ": " << x.second << " sended" << endl;
				//send to new node
				json data = {
					{"source", "node"},
					{"op", "saveData"},
					{"key", x.first},
					{"val", x.second}
				};
				//mySocket.send(data.dump());
				connections[myAddress]->send(data.dump());
				tmp.emplace_back(x.first);
			}
		}
		for (int i = 0; i < tmp.size(); i++) {
			hashTable.erase(tmp[i]);
		}
	}
}

void showInfo() {
	cout << endl;
	cout << endl;
	cout << "Responsible for keys in " << interval() << endl;
	cout << "Hash Table contains: " << endl;
	for (auto& x: hashTable)
		cout << x.first << ": " << x.second << endl;
	cout << "Clients Table contains: " << endl;
	for (auto& x: clients)
		cout << x.first << ": " << x.second << endl;
	cout << "Connections Table contains: " << endl;
	for (auto& x: connections)
		cout << x.first << ": " << x.second << endl;
	cout << "mySocket: " << myAddress << " myNeighbor: " << neighborAddress << endl;
	cout << endl;
	cout << endl;
}

void echo() {
	showInfo();
	json echo = {
		{"source", "node"},
		{"op", "echo"},
		{"init", upperBound}
	};
	if(connections[neighborAddress]->send(echo.dump(), socket::dont_wait)) {
		cout << "Message sended" << endl;
	} else {
		cout << "Error sending message" << endl;
	}
}

void nodeOps(json &req ) {
	string op, id;
	op = req["op"];
	json response;
	
	if(op == "AddNode") {
		id = req["data"];
		if(inRange(id)) {
			string nodeAdd = req["address"];
			cout << "I have your Keys" << endl;
			string tmp = lowerBound;
			json updateSucc = {
				{"source", "node"},
				{"op", "UpdateSucc"},
				{"address", nodeAdd}
			};
			if(connections[myAddress]->send(updateSucc.dump())) {
				//cout << "message: " << setw(4) << updateSucc << endl;
			} else {
				cout << "error sending message" << endl;
			}
			lowerBound = id;
			response = {
				{"op", op},
				{"data", tmp},
				{"succAddress", myAddress}
			};
			socket tmpSocekt(ctx, socket_type::push);
	    	tmpSocekt.connect(req["respAdd"]);
			if(tmpSocekt.send(response.dump())) {
				//cout << "Response: " << setw(4) << response << endl;
			} else {
				cout << "error sending message" << endl;
			}
			tmpSocekt.close();
		} else {
			cout << "Error: Not in my range, delegating..." << endl;
			if(id < lowerBound)
				connections[myAddress]->send(req.dump());
			else
				connections[neighborAddress]->send(req.dump());
		}
	} else if(op =="DeleteNode") {
		cout << "Taking over your keys" << endl;
		lowerBound = req["data"];
	} else if(op =="UpdateSucc") {
		unordered_map<string, socket *>::iterator it = connections.find(neighborAddress);
		if (it != connections.end()) {
			p.remove(*it->second);
			it->second->close();
			delete it->second;
			connections.erase(it);
		}
		neighborAddress = req["address"];

		if(neighborAddress != myAddress) {
			socket* sc = new socket(ctx, socket_type::pair);
			connections.emplace(neighborAddress, sc);
			connections[neighborAddress]->connect(neighborAddress);
			p.add(*connections[neighborAddress], poller::poll_in);
		}
		cout << "My successor changed, connected to: " << neighborAddress << endl;
	} else if (op == "sendData") {
		sendToNode();
	} else if(op == "saveData") {
		string key, val;
		key = req["key"];
		val = req["val"];
		hashTable.emplace(key,val);
		cout << "Stored Key: " << key << " Val: " << val << " at: " << upperBound << endl;
	} else if(op == "echo") {
		if (req["init"] == upperBound) {
			cout << "echo done" << endl;
			return;
		}
		else {
			showInfo();
			//mySocket.send(req.dump());
			if(connections[neighborAddress]->send(req.dump(), socket::dont_wait)) {
				cout << "Message sended" << endl;
			} else {
				cout << "Error sending message" << endl;
			}
		}
	} else {
		response = {
			{"status", "Error"},
			{"msg", "Invalid option: " + op}
		};
		cout << "Unknown option: " << op << endl;
	}
}

void handleClientRequest(json &req) {
	if(req["source"] == "node") {
		nodeOps(req);
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
			if(key < lowerBound)
				connections[myAddress]->send(req.dump());
			else
				connections[neighborAddress]->send(req.dump());
		}
	} else {
		cout << "Unkown source, ignoring message!!" << endl;
	}
}

void deleteSockets() {
	for(auto& e:clients) {
		delete e.second;
	}
	for(auto& e:connections) {
		delete e.second;
	}
}

void delegateKeys() {
	// send keys to successor
	if(myAddress == neighborAddress)
		return;
	json req = {
		{"source", "node"},
		{"op", "DeleteNode"},
		{"data", lowerBound}
	};
	//successor.send(req.dump());
	connections[neighborAddress]->send(req.dump());
	if (hashTable.empty()) {
		cout << "Nothing to send..." << endl;
	} else {
		for (auto& x: hashTable) {
			cout << x.first << ": " << x.second << " sended" << endl;
			//send to new node
			json data = {
				{"source", "node"},
				{"op", "saveData"},
				{"key", x.first},
				{"val", x.second}
			};
			connections[neighborAddress]->send(data.dump());
			//hashTable.erase(x.first);
		}
		hashTable.clear();
	}
	json updateSucc = {
		{"source", "node"},
		{"op", "UpdateSucc"},
		{"address", neighborAddress}
	};
	if(connections[myAddress]->send(updateSucc.dump(), socket::dont_wait)) {
		cout << "message: " << setw(4) << updateSucc << endl;
	} else {
		cout << "error sending message" << endl;
	}
}

int main(int argc, char** argv) {
	srand(time(NULL));
	if(argc != 5) {
		cout << "Enter bootstrap(1 or 0) myAddress neighborAddress clientsAddress" << endl;
		return EXIT_FAILURE;
	}
	char myID[SHA_DIGEST_LENGTH*2+1];
	string clientsAddress="tcp://", op;

	int bootstrap = atoi(argv[1]);
	myAddress.append(argv[2]);
	neighborAddress.append(argv[3]);
	clientsAddress.append(argv[4]);
	cout << myAddress << " " << neighborAddress << " " << clientsAddress << endl;

	socket* sc = new socket(ctx, socket_type::pair);
	connections.emplace(myAddress,sc);
	connections[myAddress]->bind(myAddress);

	getID(to_string(rand()%100), myID);
    upperBound = string(myID);

    socket clientsSocket(ctx, socket_type::pull);
	clientsSocket.bind(clientsAddress);
	cout << "Listening to clients on: " << clientsAddress << endl;

    if(bootstrap) {
    	socket* sc = new socket(ctx, socket_type::pair);
		connections.emplace(neighborAddress,sc);
		connections[neighborAddress]->connect(neighborAddress);

    	string id_msg_res;
    	json ans;
    	json id_msg = {
    		{"op", "send_id"},
    		{"data", myID}
    	};
	    //mySuccessor.send(id_msg.dump());
	    connections[neighborAddress]->send(id_msg.dump());

	    //mySocket.receive(id_msg_res);
	    connections[myAddress]->receive(id_msg_res);
	    ans = json::parse(id_msg_res);
		if(ans["op"] == "send_id") {
			lowerBound = ans["data"];
		} else {
			cout << "Unknown option: " << op << endl;
			return EXIT_FAILURE;
		}
    } else {
    	cout << "Connecting to existing chord..." << endl;
		string res;
		json ans;
    	json id_msg = {
    		{"source", "node"},
    		{"op", "AddNode"},
    		{"data", myID},
    		{"address", myAddress},
    		{"respAdd", clientsAddress}
    	};

    	socket tmp(ctx, socket_type::push);
    	tmp.connect(neighborAddress);
		tmp.send(id_msg.dump());
		tmp.close();

	    clientsSocket.receive(res);
	    ans = json::parse(res);
	    //cout << "answer: " << setw(4) << ans << endl;

	    lowerBound = ans["data"];
	    neighborAddress = ans["succAddress"];

	    socket* sc = new socket(ctx, socket_type::pair);
		connections.emplace(neighborAddress,sc);
		connections[neighborAddress]->connect(neighborAddress);

	    json send_data = {
	    	{"source", "node"},
	    	{"op", "sendData"}
	    };
	    connections[neighborAddress]->send(send_data.dump());
    }
    cout << "Listening on: " << myAddress << " and connectig to neighbor on: " << neighborAddress << endl;
	cout << "Responsible for keys in " << interval() << endl;

	int standardin = fileno(stdin);

	p.add(standardin, poller::poll_in);
	p.add(*connections[myAddress], poller::poll_in);
	p.add(*connections[neighborAddress], poller::poll_in);
	p.add(clientsSocket, poller::poll_in);

	while(true) {
		if(p.poll()) {
			if(p.has_input(standardin)) {
				string input;
				getline(cin, input);
				if(input == "q" || input == "quit" || input == "Quit" || input == "ex" || input == "Exit" || input == "exit") {
					break;
				} else if(input == "sh") {
					echo();
				}
			}
			if(p.has_input(*connections[myAddress])) {
				cout << "input from predecessor" << endl;
				string node_req;
				connections[myAddress]->receive(node_req);
				json req = json::parse(node_req);
				//cout << "req: " << setw(4) << req << endl;
				handleClientRequest(req);
			}
			if(p.has_input(*connections[neighborAddress])) {
				cout << "input from successor" << endl;
				string node_req;
				connections[neighborAddress]->receive(node_req);
				json req = json::parse(node_req);
				//cout << "req: " << setw(4) << req << endl;
				handleClientRequest(req);
			}
			if(p.has_input(clientsSocket)) {
				cout << "input from client" << endl;
				string client_req;
				clientsSocket.receive(client_req);
				json req = json::parse(client_req);
				//cout << "req: " << setw(4) << req << endl;
				handleClientRequest(req);
			}
		}
	}
	delegateKeys();
	deleteSockets();
	clientsSocket.close();
	ctx.terminate();
	return 0;
}
