#include <iostream>
#include <string>
#include <fstream>
#include <zmqpp/zmqpp.hpp>
#include <dirent.h>
#include <sys/stat.h>
#include "src/json.hpp"
#include "src/MinHeap.h"

#include <openssl/md5.h>

using namespace std;
using namespace zmqpp;

using json = nlohmann::json;

void HeapSort(MinHeap<Server> Heap) {
	Server tmp;
	if(Heap.getSize()==0) {
		cout << "Heap empty!!";
	}

	while(Heap.getSize()>0) {
		tmp = Heap.pop();
		cout << "Server Address: " << tmp.address << endl;
		cout << "Server bytes_transmitting: " << tmp.bytes_transmitting << endl;
		cout << "Server space_used: " << tmp.space_used << endl;
		cout << "Server key: " << tmp.key << endl;
		cout<<endl;
	}
	cout<<endl;
}

void loadUsersFilesInfo (json& users, json& files) {
	ifstream usr("users.json");
	usr >> users;

	ifstream fls("files.json");
	fls >> files;
}

void saveUsersFilesInfo (json& users, json& files) {
	ofstream usr("users.json");
	usr << setw(4) << users << endl;
	usr.close();

	ofstream fl("files.json");
	fl << setw(4) << files << endl;
	fl.close();

}

void listFiles(message& m, message& response, socket& s, json& users) {
	string files, username;

	m >> username;

	if(users[username].find("files") != users[username].end()) {
		// Get list of files
		for (json::iterator it = users[username]["files"].begin(); it != users[username]["files"].end(); it++) {
			files += "\t";
			files += it.key() ;
			files += "\n\t";	
		}
		files += "\n";
	} else {
		files = "No files found";
	}
	// Send list of files to Client
	response << files;
	s.send(response);
	// --F
}

void deleteFile(message& m, message& response, socket& s, json& users, json& files) {
	string username, filename, SHA1, serverLocation;
	size_t fileSize;
	message request_delete, response_delete;

	m >> username;
	m >> filename;

	// Obtain location of file to delete
	if(users[username].count("files") && users[username]["files"].count(filename)) {
		SHA1 = users[username]["files"][filename];
		serverLocation = files[SHA1]["location"];
		fileSize = files[SHA1]["size"];
		// Update Info

		// Delete association of file to User
		users[username]["files"].erase(users[username]["files"].find(filename));
		if(!users[username]["files"].size())
			users[username].erase("files");

		// Reduce owners to the file
		int owners = files[SHA1]["owners"];
		owners -= 1;
		files[SHA1]["owners"] = owners;
		// Send Message Delete file to serverLocation, if there are no more owners of the file
		if (!owners)  {
			response << SHA1 << fileSize << serverLocation;
			files.erase(files.find(SHA1));
		} else {
			response << "Done" << "File successfully deleted";
		}
		saveUsersFilesInfo(users, files);
	} else  {
		response << "Error" << "The file \"" + filename + "\" requested does not exist or couldn't be read!!";
	}
	s.send(response);
	return;
}

void updateInfo(message& request, message& response, socket& s, json& users, json& files, MinHeap<Server> &servers_queue) {
	string op, fname, username, SHA1, serverLocation;
	size_t fileSize;
	int owners = 1;
	Server tmp;
	int query;

	request >> op;

	if (op == "Upload") {
		request >> username;
		request >> fname;
		request >> SHA1;
		request >> fileSize;
		request >> serverLocation;

		// Update Priority queue (fileSize, diskSpace)
		query = servers_queue.search(serverLocation);
		if(query < 0) {
			cout << "Server not registered!!" << endl;
			response << "Error" << "Server not registered!!";
		} else {
			tmp = servers_queue.deleteAt(query);
			tmp.bytes_transmitting -= fileSize; 
			tmp.space_used += fileSize;
			tmp.key = ((double)tmp.bytes_transmitting*0.5)+((double)tmp.space_used*0.5);
			servers_queue.insert(tmp);
			response << "Updated";
		}

		// Update File and User info
		users[username]["files"][fname] = SHA1;

		files[SHA1]["owners"] = owners;
		files[SHA1]["location"] = serverLocation;
		files[SHA1]["size"] = fileSize;

		saveUsersFilesInfo(users, files);
		s.send(response);
		return;
	} else if (op == "Download") {
		request >> SHA1;
		request >> fileSize;
		request >> serverLocation;

		// Update Priority queue (fileSize, diskSpace)
		query = servers_queue.search(serverLocation);
		if(query < 0) {
			cout << "Server not registered!!" << endl;
			response << "Error" << "Server not registered!!";
			s.send(response);
		} else {
			tmp = servers_queue.deleteAt(query);
			tmp.bytes_transmitting -= fileSize; 
			tmp.key = ((double)tmp.bytes_transmitting*0.5)+((double)tmp.space_used*0.5);
			servers_queue.insert(tmp);
			response << "Updated";
		}
		s.send(response);
		return;
	} else if(op == "Delete") {
		request >> serverLocation;
		request >> fileSize;
		query = servers_queue.search(serverLocation);
		if(query < 0) {
			cout << "Server not registered!!" << endl;
			response << "Error" << "Server not registered!!";
			s.send(response);
		} else {
			tmp = servers_queue.deleteAt(query);
			tmp.space_used -= fileSize;
			tmp.key = ((double)tmp.bytes_transmitting*0.5)+((double)tmp.space_used*0.5);
			servers_queue.insert(tmp);
			response << "Updated";
		}
		s.send(response);
		return;
	} else {
		// Error notification
		response << "Error" << "Option unkown";
		s.send(response);
		return;
	}
}

void uploadFile(message& request, message& response, socket& s, json& users, json& files, MinHeap<Server> &servers_queue) {
	string fname, username, SHA1, location;
	size_t size;
	int owners = 1; // Increment file owners in one
	Server tmp;

	request >> username;
	request >> fname;
	request >> SHA1;
	request >> size;

	// Check if the file was uploaded before
	if(files.find(SHA1) != files.end()) {
		// File already exists
		// Update Info 
		//Check if file wasn't uploaded before by the user
		if(!users[username]["files"].count(fname)) {
			users[username]["files"][fname] = SHA1;
			owners = files[SHA1]["owners"];
			owners += 1;
			files[SHA1]["owners"] = owners;
			saveUsersFilesInfo(users, files);
			// Notify user
			response << "Done";
		} else {
			response << "Error" << "File " + fname + " already exists!!";
		}
		s.send(response);
		return;
	} else {
		if(users[username].count("files") && users[username]["files"].count(fname)) {
			response << "Error" << "File with name: \"" + fname + "\" already exists!!";
			s.send(response);
			return;
		}
	}

	// Find server to Upload file, according to Priority queue
	if(servers_queue.getSize() == 0) {
		response << "Error" << "No servers connected";
		s.send(response);
		return;
	}
	tmp = servers_queue.pop();
	location = tmp.address;
	tmp.bytes_transmitting += size; 
	tmp.key = ((double)tmp.bytes_transmitting*0.5)+((double)tmp.space_used*0.5);
	servers_queue.insert(tmp);
	cout << "File to be uploaded: " << SHA1 << " of size (bytes): " << size << endl;

	// Send Server info to client, and update priority queue
	response << location;
	s.send(response);
	return;
}

void downloadFile(message& request, message& response, socket& s, json& users, json& files, MinHeap<Server> &servers_queue) {
	string fname, username, SHA1, location;
	size_t fsize;
	Server tmp;
	request >> username;
	request >> fname;

	if (users[username].count("files") && users[username]["files"].count(fname)) {
		SHA1 = users[username]["files"][fname];

		//Search location of file to download
		if (files[SHA1]["location"].is_string()) {
			location = files[SHA1]["location"];
			fsize = files[SHA1]["size"];	// .get<long long int>();

			// Send Server info to client, and update priority queue
			int query = servers_queue.search(location);
			if(query < 0) {
				cout << "Server not connected!!" << endl;
				response << "Error" << "Server not connected!!";
			} else {
				tmp = servers_queue.deleteAt(query);
				tmp.bytes_transmitting += fsize;
				tmp.key = ((double)tmp.bytes_transmitting*0.5)+((double)tmp.space_used*0.5);
				servers_queue.insert(tmp);
				response << location << fsize << SHA1;			
			}
		}
	}
	else {
		// File doesn't found 
		response << "Error" << "The file \"" + fname + "\" requested does not exist or couldn't be read!!";
	}
	s.send(response);
	return;
}

void createUser(message& m, message& response, socket& s, json& users) {
	string username, password;

	m >> username;
	m >> password;

	// Check if the username exists already

	if (users.find(username) != users.end()){
		response << "Failed" << "Username \"" + username + "\" already exists!";
	}
	else {
		users[username]["password"] = password;
		//users[username]["files"] = {};
		ofstream usr("users.json");
		cout << "\nNew user created: " << username << endl;
		usr << setw(4) << users << endl;
		usr.close();
		response << "Ok" << "Username \"" + username + "\" was created!";
	}

	s.send(response);
}

void verifyUser(message& m, message& response, socket& s, json& users) {

	string username, password;

	m >> username;
	m >> password;

	// Check if the username exists already

	if (users.find(username) == users.end()){
		response << "Failed" << "Username \"" + username + "\" doesn't exists!";
	}
	else {
		if (users[username]["password"] == password) {
			response << 1;
		}
		else response << 0;
	}

	s.send(response);	
}

void addServer (message& request, message& response, socket& s, MinHeap<Server> &servers_queue) {
	//string serverLocation, diskSpace;
	Server tmp;

	request >> tmp.address;
	request >> tmp.space_used;
	request >> tmp.bytes_transmitting;
	tmp.key = ((double)tmp.bytes_transmitting*0.5)+((double)tmp.space_used*0.5);

	// Add server to the priority queue
	servers_queue.insert(tmp);
	cout << "Server " << tmp.address << " connected!!" << endl;

	response << "ok";
	s.send(response);
}

void deleteServer (message& request, message& response, socket& s, MinHeap<Server> &servers_queue) {
	string serverLocation;
	Server tmp;

	request >> serverLocation;

	// Delete server from Priority queue
	int query = servers_queue.search(serverLocation);
	if(query < 0) {
		cout << "Server not registered!!" << endl;
	} else {
		tmp = servers_queue.deleteAt(query);
		cout << "Server " << tmp.address << " removed!" << endl;
	}
	response << "ok";
	s.send(response);
}

void clientMessageHandler(message& request, message& response, socket& s, json& users, json& files, MinHeap<Server> &servers_queue) {
	string op;
	request >> op;

	cout << "Option: " << op << endl;

	if(op == "CreateUser") {
		createUser(request, response, s, users);
	} else  if(op == "Login") {
		verifyUser(request, response, s, users);
	} else  if(op == "Upload") {
		uploadFile(request, response, s, users, files, servers_queue);
	} else if(op == "Download") {
		downloadFile(request, response, s, users, files, servers_queue);
	} else if(op == "List_files") {
		listFiles(request, response, s, users);
	} else if(op == "Delete") {
		deleteFile(request, response, s, users, files);
	} else {
		response << "Error";
	}
}

void serverMessageHandler(message& request, message& response, socket& s, json& users, json& files, MinHeap<Server> &servers_queue) {
	string op;
	request >> op;

	cout << "Option from Server: " << op << endl;

	if(op == "Add") {
		addServer(request, response, s, servers_queue);
	} else  if(op == "Delete") {
		deleteServer(request, response, s, servers_queue);
	} else  if(op == "UpdateInfo") {
		updateInfo(request, response, s, users, files, servers_queue);
	} else {
		response << "Error";
	}
}

int main() {
	cout << "This is the Broker\n"; 

	context ctx;  
	socket clients_sock(ctx, socket_type::rep);
	socket servers_sock(ctx, socket_type::rep);

	MinHeap<Server> servers_queue;

	cout << "Binding socket to tcp port 5555 for clients and tcp port 5556 for servers\n";
	clients_sock.bind("tcp://*:5555");
	servers_sock.bind("tcp://*:5556");

	int standardin = fileno(stdin);
	poller p;
	p.add(clients_sock, poller::poll_in);
	p.add(servers_sock, poller::poll_in);
	p.add(standardin, poller::poll_in);
	
	// Load files and Users information

	// Users-> This will store users info: name, password and files that own
	// Files-> This will store files info: SHA1, location (which server),
	// 			quantity of users that own the file.

	json users, files;
	loadUsersFilesInfo(users, files);

	// ifstream usr("users.json");
	// usr >> users;

	// ifstream fls("files.json");
	// fls >> files;

	while(true) {
		cout << "Waiting for message to arrive!\n";
		if(p.poll()) {
			if(p.has_input(clients_sock)) {
				message request, response;
				clients_sock.receive(request);
				cout << "Message received!\n";
				clientMessageHandler(request, response, clients_sock, users, files, servers_queue);
			}
			if(p.has_input(servers_sock)) {
				message request, response;
				servers_sock.receive(request);
				cout << "Message received!\n";
				serverMessageHandler(request, response, servers_sock, users, files, servers_queue);
			}
			if(p.has_input(standardin)) {
				string input;
				getline(cin, input);
				if(input == "q" || input == "quit" || input == "Quit" || input == "ex" || input == "Exit" || input == "exit") {
					break;
				} else if(input == "sh") {
					HeapSort(servers_queue);
				}
			}
		}
	}
	clients_sock.close();
	servers_sock.close();
	ctx.terminate();
	return 0;
}