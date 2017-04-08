#include <iostream>
#include <stdio.h>
#include <string>
#include <fstream>
#include <zmqpp/zmqpp.hpp>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>			// Sleep

#include "src/json.hpp"

using json = nlohmann::json;

#include <openssl/sha.h>

#define CHUNK_SIZE 5242880

using namespace std;
using namespace zmqpp;

void getFileName(string fname, char file_name[100]) {
	char * tmp = new char[fname.size()+1];
	memcpy(tmp, fname.c_str(), fname.size()+1);
	char *temp_pt = strrchr(tmp, '/');
	if(temp_pt == NULL) {
		strcpy (file_name, tmp);
	} else {

		size_t pos = temp_pt-tmp+1;
		strcpy(file_name, &tmp[pos]);
	}
	delete [] tmp;
}

void saveSpaceDisk(json& data) {
	ofstream ds("dSpace.json");
	ds << setw(4) << data << endl;
	ds.close();
	return;
}

void updateDiskUsage (long fileSize) {
	json space;
	long usage;
	ifstream spc("dSpace.json");
	spc >> space;

	if (space.find("diskSpace") != space.end()) {
		usage = space["diskSpace"];
		usage += fileSize;
		space["diskSpace"] = usage;
	}
	else {
		usage = fileSize;
		space["diskSpace"] = usage;
	}
	saveSpaceDisk(space);
	return;
}

void deleteFile(message &m, message &response, socket &s, string client_id, string download_address, socket &broker) {
	string filename, l, ans, fileSize_str;
	long fileSize;
	message request_broker, response_broker;

	m >> filename;
	m >> fileSize_str;
	fileSize = atol(fileSize_str.c_str());

	l = "Uploads/" + filename;
	response << client_id << "" << "DeleteDone";

	cout << "Deleting: " + l << endl;;
	const char * location =  l.c_str();

	if( remove( location ) != 0 ){
		response << "Error" << "Deleting File";
	}
	else {
		updateDiskUsage(-1*fileSize);
		response << "ok";
		request_broker << "UpdateInfo" << "Delete" << download_address << fileSize_str;
		broker.send(request_broker); 
		broker.receive(response_broker);
		if(response_broker.get(0) == "Error") {
			ans = response_broker.get(1);
			cout << ans << endl;
		} else if(response_broker.get(0) == "Updated") {
			cout << "Updated on broker" << endl;
		}
	}
	s.send(response);
}

void uploadFile(message &client_request, message &server_response, string client_id, string download_address, socket &broker) {
	string fname, path, username, fname_client, ans, file_size_str;
	size_t size;
	char * data, broker_fname[100];
	FILE * f;
	int chunk_size;
	long file_size;
	message request_broker, response_broker;

	server_response << client_id << "";

	client_request >> username;
	client_request >> fname_client;
	client_request >> fname;
	path = "Uploads/";
	path.append(fname);
	cout << "File: " << path << endl;
	client_request >> file_size_str;
	file_size = atol(file_size_str.c_str());

	f = fopen(path.c_str(), "wb");
	client_request >> chunk_size;

	size = client_request.size(8);

	data = (char*)client_request.raw_data(8);
	fwrite(data, 1, size, f);
	fclose(f);

	//server_response << "UpDone" << path.substr(0,40);
	updateDiskUsage(file_size);
	getFileName(fname_client, broker_fname);
	request_broker << "UpdateInfo" << "Upload" << 1 << username << broker_fname << fname << file_size_str << download_address;

	broker.send(request_broker); 
	broker.receive(response_broker);
	if(response_broker.get(0) == "Error") {
		ans = response_broker.get(1);
		cout << ans << endl;
	} else if(response_broker.get(0) == "Updated") {
		cout << "Updated on broker" << endl;
	}
	
	//server_response << download_address;
	//s.send(server_response);
	return;

}

void downloadFile(message &client_request, message &server_response, socket &s, string client_id, string download_address, socket &broker) {
	string op = "Download", sz_str, nPart, fname, path, client_fname, ans;
	FILE* f;
	char *data;
	size_t size;
	message request_broker, response_broker;

	client_request >> client_fname;
	client_request >> fname;
	client_request >> nPart;
	//cout << nPart;
	client_request >> sz_str;


	path = "Uploads/";

	path.append(fname+"."+nPart);

	if(!(f = fopen(path.c_str(), "rb"))) {
		server_response << client_id << "" << "Error" << "The file \"" + fname + "\" requested does not exist or couldn't be read!!" << download_address;
		s.send(server_response);
		return;
	}

	server_response << client_id << "" << op << client_fname << fname;
	fflush(f);

	fseek(f, 0, SEEK_SET);

	data = (char *) malloc (CHUNK_SIZE);
	assert(data);

	size = fread (data, 1, CHUNK_SIZE, f);

	cout <<"File size in bytes: " << size << endl;

	server_response << nPart;
	server_response << to_string(size);

	server_response << CHUNK_SIZE;
	server_response.push_back(data, size);
	server_response.push_back(download_address);

	fclose(f);
	free(data);
	sz_str = to_string(size);
		
	request_broker << "UpdateInfo" << "Download" << fname << sz_str << download_address;

	broker.send(request_broker); 
	broker.receive(response_broker);
	if(response_broker.get(0) == "Error") {
		ans = response_broker.get(1);
		cout << ans << endl;
	} else if(response_broker.get(0) == "Updated") {
		cout << "Updated on broker" << endl;
	}

	s.send(server_response);
	return;
}

void messageHandler(message &client_request, message &server_response, socket &s, string socket_address, socket &broker) {
	string op, client_id, empty;
	client_request >> client_id >> empty >> op;

	cout << "Option: " << op << endl;

	if(op == "Upload") {
		uploadFile(client_request, server_response, client_id, socket_address, broker);
	} else if(op == "Download") {
		downloadFile(client_request, server_response, s, client_id, socket_address, broker);
	} else if(op == "Delete") {
		deleteFile(client_request, server_response, s, client_id, socket_address, broker);
	} else {
		server_response << client_id << "" << "Error" << "Invalid option";
		s.send(server_response);
	}
}

void registerToBroker(socket &broker, string address) {
	json space;
	long usage, bytes;
	ifstream spc("dSpace.json");
	
	if(spc.good()) 
		spc >> space;
	if(space.find("diskSpace") != space.end()) {
		usage = space["diskSpace"];
	}
	else {
		usage = 0;
		space["diskSpace"] = usage;
		saveSpaceDisk(space);
	}
	bytes = 0;
	message broker_register;
	broker_register << "Add" << address << to_string(usage) << to_string(bytes);
	broker.send(broker_register);
}

void disconnectFromBroker(socket &broker, string address) {
	message broker_disconnect, response;
	string ans;

	broker_disconnect << "Delete" << address;
	broker.send(broker_disconnect);
	broker.receive(response);
	response >> ans;
	cout << ans << endl;				
}

int main(int argc, char* argv[]) {
	string broker_address = "tcp://";
	string download_address = "tcp://";
	string response;

	if (argc != 3) {
		cout << "Please use like this: ./fileServer address:port broker_address:port" << endl;;
		return EXIT_FAILURE;
	} else {
		download_address.append(argv[1]);
		broker_address.append(argv[2]);
	}

	cout << "This is the server\n"; 

	context ctx;  
	socket s(ctx, socket_type::req);
	socket down(ctx, socket_type::router);

	int standardin = fileno(stdin);

	cout << "Binding socket to tcp port " << download_address << endl;
	s.connect(broker_address);
	registerToBroker(s, download_address);
	down.bind(download_address);

	poller p;

	p.add(s, poller::poll_in);
	p.add(down, poller::poll_in);
	p.add(standardin, poller::poll_in);

	struct stat sb;
	lstat("Uploads/", &sb);

	if(!S_ISDIR(sb.st_mode)) {
		string url = "mkdir -p Uploads";
		const char * directory =  url.c_str();
		system(directory);
	}

	while(true) {
		cout << "Waiting for message to arrive!\n";
		if(p.poll()) {
			if(p.has_input(down)) {

				message client_request, server_response;
				down.receive(client_request);

				cout << "Message received!\n";

				messageHandler(client_request, server_response, down, download_address, s);
			}
			if(p.has_input(s)) {

				message broker_response;
				s.receive(broker_response);

				cout << "Message received!\n";
				broker_response >> response;
				cout << response << endl;

				//messageHandler(client_request, server_response, s, "");
			}
			if(p.has_input(standardin)) {
				string input;
				getline(cin, input);
				if(input == "q" || input == "quit" || input == "Quit" || input == "ex" || input == "Exit" || input == "exit") {
					break;
				}
			}
		}
	}
	disconnectFromBroker(s, download_address);
	s.close();
	down.close();
	ctx.terminate();

	return 0;
}
