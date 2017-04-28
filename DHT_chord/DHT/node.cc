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
static string lowerBound = "", upperBound = "", myAddress = "", neighborAddress = "";

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

void sendToNode(string nodeAdd) {
	socket tmp(ctx, socket_type::push);
	tmp.connect("tcp://localhost:" + nodeAdd);	
	for (auto& x: hashTable) {
		if(!inRange(x.first)) {
			cout << x.first << ": " << x.second << endl;
			//send to new node
			json data = {
				{"source", "node"},
				{"op", "saveData"},
				{"key", x.first},
				{"val", x.second}
			};
			tmp.send(data.dump());
			hashTable.erase(x.first);
		}
	}
	tmp.close();
}


void echo(socket &successor) {
	cout << "Responsible for keys in " << interval() << endl;
	cout << "Hash Table contains: " << endl;
	for (auto& x: hashTable)
		cout << x.first << ": " << x.second << endl;
	cout << "Clients Table contains: " << endl;
	for (auto& x: clients)
		cout << x.first << ": " << x.second << endl;

	json echo = {
		{"source", "node"},
		{"op", "echo"},
		{"init", upperBound}
	};
	successor.send(echo.dump());
}

void echo(socket &successor, json &req) {
	cout << "Responsible for keys in " << interval() << endl;
	cout << "Hash Table contains: " << endl;
	for (auto& x: hashTable)
		cout << x.first << ": " << x.second << endl;
	cout << "Clients Table contains: " << endl;
	for (auto& x: clients)
		cout << x.first << ": " << x.second << endl;

	successor.send(req.dump());
}

void nodeOps(json &req, socket &successor, socket &mySocket) {
	string op, id;//nodeAdd;
	op = req["op"];
	json response;
	
	if(op == "AddNode") {
		if(inRange(id)) {
			id = req["data"];
			string nodeAdd = req["address"];
			cout << "I have your Keys" << endl;
			string tmp = lowerBound;
			lowerBound = id;
			response = {
				{"op", op},
				{"data", tmp},
				{"succAddress", myAddress}
			};
			socket tmpSocekt(ctx, socket_type::pair);
	    	tmpSocekt.connect("tcp://localhost:" + nodeAdd);
			tmpSocekt.send(response.dump());
			tmpSocekt.disconnect("tcp://localhost:" + nodeAdd);
			tmpSocekt.close();
			json updateSucc = {
				{"source", "node"},
				{"op", "UpdateSucc"},
				//{"data", tmp},
				//{"data", id},
				{"address", nodeAdd}
			};
			mySocket.send(updateSucc.dump());
			//successor.send(updateSucc.dump());
		} else {
			cout << "Error: Not in my range, delegating..." << endl;
			successor.send(req.dump());
		}
	} else if(op =="DeleteNode") {
		cout << "Taking over your keys" << endl;
	} else if(op =="UpdateSucc") {
		//id = req["data"];
		cout << op << endl;
		string nodeAdd = req["address"];
		//if(id == upperBound) {
			string disc = "tcp://localhost:" + neighborAddress;
			successor.disconnect(disc);
			successor.connect("tcp://localhost:" + nodeAdd);
			neighborAddress = nodeAdd;
			cout << "My successor changed, connected to: " << nodeAdd << endl;
		//} else {
		//	cout << "Error: Not me, delegating..." << endl;
		//	successor.send(req.dump());	
		//}
	} else if (op == "sendData") {
		string nodeAdd = req["address"];
		sendToNode(nodeAdd);
	} else if(op == "saveData") {
		string key, val;
		key = req["key"];
		val = req["val"];
		hashTable.emplace(key,val);
		cout << "Stored Key: " << key << " Val: " << val << " at: " << upperBound << endl;
	} else if(op == "echo") {
		if (req["init"] == upperBound) {
			//cout << "Recvd: " << req["init"] << "Mine: " << upperBound << endl;
			//assert(req["init"] == upperBound);
			cout << "echo done" << endl;
			return;
		}
		else {
			//cout << "Recvd: " << req["init"] << "Mine: " << upperBound << endl;
			//assert(req["init"] == upperBound);
			echo(successor, req);
		}
	} else {
		response = {
			{"status", "Error"},
			{"msg", "Invalid option: " + op}
		};
		cout << "Unknown option: " << op << endl;
	}
}

void handleClientRequest(json &req, socket &successor, socket &mySocket) {
	if(req["source"] == "node") {
		nodeOps(req, successor, mySocket);
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
		cout << "Enter bootstrap(1 or 0) myAddress neighborAddress clientsAddress" << endl;
		return EXIT_FAILURE;
	}
	char myID[SHA_DIGEST_LENGTH*2+1];
	string /*myAddress, neighborAddress,*/ clientsAddress, op;

	int bootstrap = atoi(argv[1]);
	myAddress = argv[2];
	neighborAddress = argv[3];
	clientsAddress = argv[4];

	socket mySocket(ctx, socket_type::pair);
	mySocket.bind("tcp://*:" + string(myAddress));
	//address = "tcp://localhost:" + string(myAddress);
	//address = string(myAddress);

	socket mySuccessor(ctx, socket_type::pair);

	getID(to_string(rand()%100), myID);
    upperBound = string(myID);

    socket clientsSocket(ctx, socket_type::pull);
	clientsSocket.bind("tcp://*:" + string(clientsAddress));
	cout << "Listening to clients on port " << clientsAddress << endl;

    if(bootstrap) {
    	mySuccessor.connect("tcp://localhost:" + string(neighborAddress));
    	//succAddress = "tcp://localhost:" + string(successorAddress);
    	//succAddress = string(neighborAddress);

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
		cout << "Listening on " << myAddress << " and connectig to neighbor on " << neighborAddress << endl;
		cout << "Responsible for keys in " << interval() << endl;
    } else {
    	cout << "Connecting to existing chord..." << endl;
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
    		{"address", myAddress}
    	};

    	socket tmp(ctx, socket_type::push);
    	tmp.connect("tcp://localhost:" + string(neighborAddress));
		tmp.send(id_msg.dump());
		tmp.close();

	    mySocket.receive(res);
	    ans = json::parse(res);

	    lowerBound = ans["data"];
	    //mySuccessor.disconnect("tcp://localhost:" + succAddress);
	    string strTmp = ans["succAddress"];
	    mySuccessor.connect("tcp://localhost:" + strTmp);
	    neighborAddress = strTmp;
	    json send_data = {
	    	{"source", "node"},
	    	{"op", "sendData"},
	    	{"address", string(clientsAddress)}
	    };
	    mySuccessor.send(send_data.dump());

	    cout << "Listening on " << myAddress << " and connected to neighbor on " << strTmp << endl;
	    cout << "Responsible for keys in " << interval() << endl;

	    /*lower_bound, mysuccesor*/
    }

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
					/*cout << "Responsible for keys in " << interval() << endl;
					cout << "Hash Table contains: " << endl;
					for (auto& x: hashTable)
						cout << x.first << ": " << x.second << endl;
					cout << "Clients Table contains: " << endl;
					for (auto& x: clients)
						cout << x.first << ": " << x.second << endl;
					json echo = {
						{"source", "node"},
						{"op", "echo"},
						{"init", upperBound}
					};
					mySuccessor.send(echo.dump());*/
					echo(mySuccessor);
				}
			}
			if(p.has_input(mySocket)) {
				cout << "input from node" << endl;
				string node_req;
				mySocket.receive(node_req);
				json req = json::parse(node_req);
				handleClientRequest(req, mySuccessor, mySocket);
			}
			if(p.has_input(clientsSocket)) {
				cout << "input from client" << endl;
				string client_req;
				clientsSocket.receive(client_req);
				json req = json::parse(client_req);
				handleClientRequest(req, mySuccessor, mySocket);
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
