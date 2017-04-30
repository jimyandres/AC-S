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
		return (key >= lowerBound || key < upperBound);
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
	tmp.connect(nodeAdd);	
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

void showInfo() {
	cout << endl;
	cout << endl;
	cout << "********************************************************************************" << endl;
	cout << "Responsible for keys in " << interval() << endl;
	cout << "Hash Table contains: " << endl;
	for (auto& x: hashTable)
		cout << x.first << ": " << x.second << endl;
	cout << "Clients Table contains: " << endl;
	for (auto& x: clients)
		cout << x.first << ": " << x.second << endl;
	cout << "mySocket: " << myAddress << " myNeighbor: " << neighborAddress << endl;
	cout << endl;
	cout << endl;
}

void echo(socket &successor) {
	showInfo();
	json echo = {
		{"source", "node"},
		{"op", "echo"},
		{"init", upperBound}
	};
	successor.send(echo.dump());
}

/*void echo(socket &successor, json &req) {
	cout << "Responsible for keys in " << interval() << endl;
	cout << "Hash Table contains: " << endl;
	for (auto& x: hashTable)
		cout << x.first << ": " << x.second << endl;
	cout << "Clients Table contains: " << endl;
	for (auto& x: clients)
		cout << x.first << ": " << x.second << endl;
	cout << "mySocket: " << myAddress << " myNeighbor: " << neighborAddress << endl;

	successor.send(req.dump());
}*/

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
			json updateSucc = {
				{"source", "node"},
				{"op", "UpdateSucc"},
				//{"data", tmp},
				//{"data", id},
				{"address", nodeAdd}
			};
			if(mySocket.send(updateSucc.dump())) {
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
			//tmpSocekt.disconnect(req["respAdd"]);
			tmpSocekt.close();
			//successor.send(updateSucc.dump());
		} else {
			cout << "Error: Not in my range, delegating..." << endl;
			successor.send(req.dump());
		}
	} else if(op =="DeleteNode") {
		cout << "Taking over your keys" << endl;
	} else if(op =="UpdateSucc") {
		//id = req["data"];
		//cout << op << endl;
		//string nodeAdd = req["address"];
		//if(id == upperBound) {
			//string disc = "tcp://localhost:" + neighborAddress;
			//cout << neighborAddress << endl;
			successor.connect(req["address"]);
			successor.disconnect(neighborAddress);
			neighborAddress = req["address"];
			cout << "My successor changed, connected to: " << neighborAddress << endl;
		/*} else {
			cout << "Error: Not me, delegating..." << endl;
			successor.send(req.dump());	
		}*/
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
			//showInfo();
			cout << "echo done" << endl;
			return;
		}
		else {
			showInfo();
			//cout << "Recvd: " << req["init"] << "Mine: " << upperBound << endl;
			//assert(req["init"] == upperBound);
			if(successor.send(req.dump(), socket::dont_wait)) {
				cout << "Message sended" << endl;
			} else {
				cout << "Error sending message" << endl;
			}
			//mySocket.send(req.dump());
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
	string /*myAddress, neighborAddress,*/ clientsAddress="tcp://", op;

	int bootstrap = atoi(argv[1]);
	myAddress.append(argv[2]);
	neighborAddress.append(argv[3]);
	clientsAddress.append(argv[4]);
	cout << myAddress << " " << neighborAddress << " " << clientsAddress << endl;

	socket mySocket(ctx, socket_type::pair);
	mySocket.bind(myAddress);
	//address = "tcp://localhost:" + string(myAddress);
	//address = string(myAddress);

	socket mySuccessor(ctx, socket_type::pair);

	getID(to_string(rand()%100), myID);
    upperBound = string(myID);

    socket clientsSocket(ctx, socket_type::pull);
	clientsSocket.bind(clientsAddress);
	cout << "Listening to clients on: " << clientsAddress << endl;

    if(bootstrap) {
    	mySuccessor.connect(neighborAddress);
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
		//cout << "Listening on: " << myAddress << " and connectig to neighbor on: " << neighborAddress << endl;
		//cout << "Responsible for keys in " << interval() << endl;
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
    		{"address", myAddress},
    		{"respAdd", clientsAddress}
    	};

    	socket tmp(ctx, socket_type::push);
    	tmp.connect(neighborAddress);
		tmp.send(id_msg.dump());
		tmp.close();

	    //mySocket.receive(res);
	    clientsSocket.receive(res);
	    ans = json::parse(res);
	    //cout << "answer: " << setw(4) << ans << endl;

	    lowerBound = ans["data"];
	    //mySuccessor.disconnect("tcp://localhost:" + succAddress);
	    //string strTmp = ans["succAddress"];
	    //cout << "successor address: " << ans["succAddress"] << endl;
	    mySuccessor.connect(ans["succAddress"]);
	    neighborAddress = ans["succAddress"];
	    json send_data = {
	    	{"source", "node"},
	    	{"op", "sendData"},
	    	{"address", clientsAddress}
	    };
	    mySuccessor.send(send_data.dump());

	    //cout << "Listening on " << myAddress << " and connected to neighbor on " << neighborAddress << endl;
	    //cout << "Responsible for keys in " << interval() << endl;

	    /*lower_bound, mysuccesor*/
    }
    cout << "Listening on: " << myAddress << " and connectig to neighbor on: " << neighborAddress << endl;
	cout << "Responsible for keys in " << interval() << endl;

	int standardin = fileno(stdin);
	poller p;

	p.add(standardin, poller::poll_in);
	p.add(mySocket, poller::poll_in);
	p.add(mySuccessor, poller::poll_in);
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
				cout << "input from predecessor" << endl;
				string node_req;
				mySocket.receive(node_req);
				json req = json::parse(node_req);
				//cout << "req: " << setw(4) << req << endl;
				handleClientRequest(req, mySuccessor, mySocket);
			}
			if(p.has_input(mySuccessor)) {
				cout << "input from successor" << endl;
				string node_req;
				mySuccessor.receive(node_req);
				json req = json::parse(node_req);
				//cout << "req: " << setw(4) << req << endl;
				handleClientRequest(req, mySuccessor, mySocket);
			}
			if(p.has_input(clientsSocket)) {
				cout << "input from client" << endl;
				string client_req;
				clientsSocket.receive(client_req);
				json req = json::parse(client_req);
				//cout << "req: " << setw(4) << req << endl;
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
