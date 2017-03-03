#include <iostream>
#include <stdio.h>
#include <string>
#include <fstream>
#include <zmqpp/zmqpp.hpp>
#include <zmq.h>
#include <dirent.h>
#include <sys/stat.h>

#include <json/json.h>
#include <json/writer.h>

#include <openssl/sha.h>

#define CHUNK_SIZE 5242880

using namespace std;
using namespace zmqpp;

void printChecksum (unsigned char * check_sum) {
	char mdString[SHA_DIGEST_LENGTH*2+1];
 
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
         sprintf(&mdString[i*2], "%02x", (unsigned int)check_sum[i]);
 
    printf("%s\n", mdString);
}

void listFiles(message &m, message &response, socket &s, string client_id) {
	string files, user, path;

	m >> user;

	DIR *dir;
	struct dirent *ent;
	path = "Uploads/" + user;
	const char * url =  path.c_str();
	if ((dir = opendir (url)) != NULL) {
		/* print all the files and directories within directory */
		while ((ent = readdir (dir)) != NULL) {
			files += "\t";
			files += ent->d_name;
			files += "\n";
			//printf ("%s\n", ent->d_name);
		}
		files += "\n";
		closedir (dir);

	} else {
	  /* could not open directory */
		perror ("");
	  //return EXIT_FAILURE;
	}
	response << client_id << "" << files;
	s.send(response);

}

void deleteFile(message &m, message &response, socket &s, string client_id) {
	string user, filename, l;

	m >> user;
	m >> filename;

	l = "Uploads/" + user + "/" + filename;
	response << client_id << "";

	cout << "Deleting: " + l << endl;;
	const char * location =  l.c_str();

	if( remove( location ) != 0 ){
		response << "Error deleting file\n";
	}
	else {
		response << "File successfully deleted\n";
	}
	s.send(response);
}

void uploadFile(message &client_request, message &server_response, socket &s, string client_id) {
	string fname, path, username;
	size_t size, total, size_sha1;
	char * data, *data_sha1 = NULL;
	FILE * f;
	int chunk_size;
	long sz;

	unsigned char check_sum[SHA_DIGEST_LENGTH];
	unsigned char *received_check_sum;

	client_request >> username;
	client_request >> fname;

	path = "Uploads/" + username + "/";

	path.append(fname);

	cout << "Path: " << path.c_str() << endl;
	server_response << client_id << "";

	if(client_request.get(5) == "Check") {
		received_check_sum = (unsigned char *)client_request.raw_data(6);
		cout << "Received check sum: ";
		printChecksum(received_check_sum);

		if(!(f = fopen(path.c_str(), "rb"))) {
			server_response << "Error" << 0;
			s.send(server_response);
			return;
		}

		fflush(f);
		fseek(f, 0L, SEEK_END);
		sz = ftell(f);
		fseek(f, 0L, SEEK_SET);

		data_sha1 = (char*) malloc (sizeof(char)*sz);
		size_sha1 = fread(data_sha1, 1, sz, f);

		SHA1((unsigned char *)data_sha1, size_sha1, (unsigned char *)&check_sum);
		free(data_sha1);
		fclose(f);

		cout << "Calculated check sum: ";
		printChecksum(check_sum);

		if(memcmp(received_check_sum, check_sum, SHA_DIGEST_LENGTH)) {
			if( remove(path.c_str()) != 0 )
				cout << "Error deleting file: " << path << endl;
			else
				cout << "File: " << path << " successfully deleted\n";

			cout << "Error" << endl;
			server_response << "Error" << 1;
			s.send(server_response);
			fclose(f);
			return;
		}
		cout << "File saved!" << endl;

		server_response << "ok";
		s.send(server_response);
		return;
	}

	client_request >> chunk_size;
	client_request >> total;

	if (total == 0)
		f = fopen(path.c_str(), "wb");
	else {
		f = fopen(path.c_str(), "ab");
		fseek(f, 0L, SEEK_END);
	}

	assert(f);
	fflush(f);

	size = client_request.size(7);

	data = (char*)client_request.raw_data(7);
	fwrite(data, 1, size, f);
	fclose(f);

	if (size == 0 || (int)size < chunk_size) {
		server_response << "Done";
		s.send(server_response);
	} else {
		cout << "Saving file...\n";
		server_response << "ok";
		s.send(server_response);
	}	
}

void downloadFile(message &client_request, message &server_response, socket &s, string client_id) {
	FILE* f;
	char *data_sha1 = NULL, *data;
	size_t size, size_sha1, offset;
	long sz;

	unsigned char check_sum[SHA_DIGEST_LENGTH];

	string fname, path, username;

	client_request >> username;
	client_request >> fname;
	client_request >> offset;

	path = "Uploads/" + username + "/";

	path.append(fname);

	if(!(f = fopen(path.c_str(), "rb"))) {

		cout << "Error" << endl;
		server_response << client_id << "" << "Error" << 0;
		s.send(server_response);
		return;
	}

	server_response << client_id << "" << fname;
	fflush(f);
	fseek(f, 0L, SEEK_END);
	sz = ftell(f);
	fseek(f, 0L, SEEK_SET);
	if(offset == 0) {
		cout <<"File size in bytes: " << sz << endl;
		server_response << sz;
	} else if((int)offset == sz) {
		data_sha1 = (char*) malloc (sizeof(char)*sz);
		size_sha1 = fread(data_sha1, 1, sz, f);

		SHA1((unsigned char *)data_sha1, size_sha1, (unsigned char *)&check_sum);
		free(data_sha1);

		cout << "Calculated check sum: ";
		printChecksum(check_sum);

		server_response.push_back(check_sum, SHA_DIGEST_LENGTH);

		fclose(f);

		if(s.send(server_response)) {
			cout << "File \"" << fname << "\" has been sended successfully\n";
		} else {
			cout << "File \"" << fname << "\"Data\" hasnt been sended successfully\n";
		}
		return;
	}

	fseek(f, offset, SEEK_SET);

	data = (char *) malloc (CHUNK_SIZE);
	assert(data);

	size = fread (data, 1, CHUNK_SIZE, f);

	server_response << CHUNK_SIZE;
	server_response.push_back(data, size);

	fclose(f);
	free(data);
	s.send(server_response);
}

void createUser(message &m, message &response, socket &s, string client_id) {
	string user, password, url;

	m >> user;
	m >> password;

	Json::Value root;   // will contains the root value after parsing.
	Json::Reader reader;
	Json::StyledStreamWriter writer;
	std::ifstream test("users.json");
	bool parsingSuccessful = reader.parse( test, root );
	if ( !parsingSuccessful )
	{
        // report to the user the failure and their locations in the document.
		std::cout  << "Failed to parse configuration: "<< reader.getFormattedErrorMessages();
	}
	if (root.isMember(user)) {
		response << client_id << "" << "Failed" << "Username \"" + user + "\" already exists!\n";
	}
	else {
		root[user]["password"] = password;
		test.close();
		ofstream test1("users.json");
		cout << "New user created: " << user << endl;
		writer.write(test1,root);

		url = "mkdir -p Uploads/" + user;
		const char * directory =  url.c_str();
		system(directory);
		response << client_id << "" << "Ok" << "Username \"" + user + "\" was created!";
	}

	s.send(response);

}

void verifyUser(message &m, message &response, socket &s, string client_id) {

	string user, password;

	m >> user;
	m >> password;

	Json::Value root;   // will contains the root value after parsing.
	Json::Reader reader;
	Json::StyledStreamWriter writer;
	std::ifstream test("users.json");
	bool parsingSuccessful = reader.parse( test, root );
	if ( !parsingSuccessful )
	{
        // report to the user the failure and their locations in the document.
		std::cout  << "Failed to parse configuration: "<< reader.getFormattedErrorMessages();
	}
	if (root[user]["password"] == password) {
		response << client_id << "" << 1;
	}
	else response << client_id << "" << 0;

	s.send(response);
}

void messageHandler(message &client_request, message &server_response, socket &s) {
	string op, client_id, empty;
	client_request >> client_id >> empty >> op;

	cout << "Option: " << op << endl;

	if(op == "CreateUser") {
		createUser(client_request, server_response, s, client_id);
	} else  if(op == "Login") {
		verifyUser(client_request, server_response, s, client_id);
	} else  if(op == "Upload") {
		uploadFile(client_request, server_response, s, client_id);
	} else if(op == "Download") {
		downloadFile(client_request, server_response, s, client_id);
	} else if(op == "List_files") {
		listFiles(client_request, server_response, s, client_id);
	} else if(op == "Delete") {
		deleteFile(client_request, server_response, s, client_id);
	} else {
		server_response << client_id << "" << "Error";
	}
}

int main(int argc, char* argv[]) {
	string server_address = "tcp://";

	if (argc != 2) {
		cout << "Please use like this: ./fileServer localhost:5555\n";
		return EXIT_FAILURE;
	} else {
		server_address.append(argv[1]);
	}

	cout << "This is the server\n"; 

	context ctx;  
	socket s(ctx, socket_type::router);
	socket down(ctx, socket_type::router);

	zmq_pollitem_t standardin;
	standardin.socket = NULL;
	standardin.fd = 0;
	standardin.events = poller::poll_in;

	cout << "Binding socket to tcp port 5555\n";
	s.bind(server_address);
	down.bind("tcp://*:5556");

	poller p;

	p.add(s, poller::poll_in);
	p.add(down, poller::poll_in);
	p.add(standardin);

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
			if(p.has_input(s)) {

				message client_request, server_response;
				s.receive(client_request);

				cout << "Message received!\n";

				messageHandler(client_request, server_response, s);
			}
			if(p.has_input(down)) {

				message client_request, server_response;
				down.receive(client_request);

				cout << "Message received!\n";

				messageHandler(client_request, server_response, down);
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

	s.close();
	down.close();
	ctx.terminate();

	return 0;
}
