#include <iostream>
#include <string>
#include <zmqpp/zmqpp.hpp>

#include <sys/stat.h>

#define CHUNK_SIZE 250000

using namespace std;
using namespace zmqpp;

void printMenu() {
	cout << "\n\n***********************************\n";
	cout << "Enter action to perform: \n\tList_files\n\tDownload\n\tUpload\n\tDelete\n\tExit\n";
	cout << "\n";
}

void downloadFile(socket &s, string op) {
	char fname[100];
	string file_name, path;
	message metadata_file, answer, data_request;
	size_t size;
	char *data;
	FILE* f;

	cout << "Enter file name: \n";
	cin.getline(fname, sizeof(fname));
	cin.getline(fname, sizeof(fname));

	metadata_file << op << fname;
	s.send(metadata_file);

	s.receive(answer);

	answer >> size;
	answer >> file_name;

	data = (char*) malloc (sizeof(char)*size);
	assert(data);

	cout << "File size: " << size << endl;

	//Check if "Downloads/" dir exists,
	//	if not, it's created.
	struct stat sb;
	lstat("Downloads/", &sb);

	if(!S_ISDIR(sb.st_mode)) {
		string url = "mkdir -p Downloads";
		const char * directory =  url.c_str();
		system(directory);
	}

	path = "Downloads/";

	path.append(file_name);

	cout << "Path: " << path << endl;

	f = fopen(path.c_str(), "wb");


	assert(f);
	fseek(f, 0L, SEEK_SET);

	data_request << "FileData" << fname << size;
	//s.connect("tcp://localhost:5555");
	s.send(data_request);

	if(s.receive_raw(data, size)) {
		cout << "Message that contains \"Data\" has been received successfully\n";
	} else {
		cout << "Message that contains \"Data\" hasnt been received successfully\n";
	}

	cout << "Saving to file..." << endl;

	fwrite(data, 1, size, f);
	fclose(f);
	free(data);

	cout << "File saved successfully\n";
}

void uploadFile(socket &s, string op) {
	FILE* f;
	char *data;
	size_t size;
	char fname[100];

	cout << "Enter file name: \n";
	cin.getline(fname, sizeof(fname));
	cin.getline(fname, sizeof(fname));
/*
	f = fopen(fname.c_str(), "rb");
	assert(f);

	if(ready_flag) {

		client_request >> size;
		data = (char*) malloc (sizeof(char)*size);
		assert(data);

		size = fread(data, 1, size, f);

		if(s.send_raw(data, size)) {
			cout << "Message that contains \"Data\" has been sended successfully\n";
		} else {
			cout << "Message that contains \"Data\" hasnt been sended successfully\n";
		}

		fclose(f);
		free(data);
		return;
	}

	fseek(f, 0L, SEEK_END);
	long sz = ftell(f);
	cout <<"File size in bytes: " << sz << endl;
	fseek(f, 0L, SEEK_SET);
	
	server_response << sz << fname;
	
	cout << "File requested: " << fname << endl;

	if(s.send(server_response, true)) {
		cout << "Message that contains \"Response\" has been sended successfully\n";
	} else {
		cout << "Message that contains \"Response\" hasnt been sended successfully\n";
	}
	fclose(f);*/
}

void listFiles(socket &s, string op, string username) {
	message request, response;
	string files;

	request << op << username;
	s.send(request);
	s.receive(response);

	response >> files;
	cout << "Files:" << endl;
	cout << files;
}

void deleteFile(socket &s, string op, string username) {
	message request, response;
	string filename, status;

	cout << "Enter File name: \n";
	cin >> filename;

	request << op << username << filename;
	s.send(request);
	s.receive(response);

	response >> status;
	cout << "Deleting file \"" << filename << "\": " << status << endl;
}

int main() {

	message login, response, create_user;
	string op, create, answer;
	char username[40], password[100];
	int access;

	cout << "This is the client\n";

	context ctx;
	socket s(ctx, socket_type::req);

	cout << "Connecting to tcp port 5555\n";
	s.connect("tcp://localhost:5555");

	while(true) {
		cout << "Create new account (yes/no): ";
		cin >> create;

		if (create == "yes" || create == "y") {
			cout << "Enter User name: \n";
			cin >> username;

			cout << "Enter Password: \n";
			cin >> password;

			create_user << "CreateUser" << username << password;
			s.send(create_user);
			s.receive(response);

			response >> op;
			response >> answer;

			cout << answer;

			if (op == "Ok") {
				break;
			}

		} else if (create == "no" || create == "n") {
			cout << "Enter User name: \n";
			cin >> username;

			cout << "Enter Password: \n";
			cin >> password;
			break;

		} else cout << "Invalid option!\n";
	}


	login << "Login" << username << password;

	s.send(login);
	s.receive(response);

	response >> access;

	if (access) {
		while(true) {
			printMenu();
			cout <<  "Enter action to perform: \n";
			cin >> op;

			if(op == "Upload" || op == "upload") {
				uploadFile(s, op);
			} else if(op == "Download" || op == "download") {
				downloadFile(s, op);
			} else if(op == "List_files" || op == "list_files") {
				listFiles(s, op, username);
			} else if(op == "Delete" || op == "delete") {
				deleteFile(s, op, username);
			} else if(op == "Exit" || op == "exit") {
				break;
			} else {
				cout << "Invalid option, please enter one of the listed options\n";
			}
		}
	}
	else cout << "User not found";

   	cout << "Finished\n";
    s.close();
    return 0;
}