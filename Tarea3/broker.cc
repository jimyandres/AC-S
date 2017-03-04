#include <iostream>
#include <string>
#include <fstream>
#include <zmqpp/zmqpp.hpp>
#include <dirent.h>
#include <sys/stat.h>
#include "src/json.hpp"

#include <openssl/md5.h>

using namespace std;
using namespace zmqpp;

using json = nlohmann::json;

// void printChecksum (const unsigned char * check_sum) {
// 	char mdString[33];
 
//     for(int i = 0; i < 16; i++)
//          sprintf(&mdString[i*2], "%02x", (unsigned int)check_sum[i]);
 
//     printf("%s\n", mdString);
// }

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

	ofstream fl("users.json");
	fl << setw(4) << users << endl;
	fl.close();

}

void listFiles(message& m, message& response, socket& s, json& users) {
	string files, username;

	m >> username;

	// Get list of files
	for (json::iterator it = users[username]["files"].begin(); it != users[username]["files"].end(); it++) {
		files += "\t";
		files += it.key() ;
		files += "\t";	
	}
	files += "\n";

	// Send list of files to Client

	// --F

	return;
}

void deleteFile(message& m, message& response, socket& s, json& users, json& files) {
	string username, filename, SHA1, serverLocation;

	m >> username;
	m >> filename;

	// Obtain location of file to delete
	SHA1 = users[username]["files"][filename];
	serverLocation = files[SHA1]["location"];


	// Update Info

	// Delete association of file to User
	users[username]["files"].erase(users[username]["files"].find(filename));

	// Reduce owners to the file
	int owners = files[SHA1]["owners"];
	owners -= 1;

	// Send Message Delete file to serverLocation, if there are no more owners of the file
	if (!owners)  {

		// --F

		files.erase(files.find(SHA1));
	}

	saveUsersFilesInfo(users, files);

	// Send Ok message to Client

	// --F

	return;
}

void updateInfo(message& request, message& response, socket& s, json& users, json& files) {
	string op, fname, username, SHA1, serverLocation;
	size_t fileSize, diskSpaceAvailable;
	int owners = 1;

	request >> op;

	if (op == "Upload") {
		request >> username;
		request >> fname;
		request >> SHA1;
		request >> fileSize;
		request >> serverLocation;

		// Update Priority queue 

		// --F

		// Update File and User info
		users[username]["files"][fname] = SHA1;

		files[SHA1]["owners"] = owners;

		saveUsersFilesInfo(users, files);

		// Notify user

		// --F

		return;
	}
	else if (op == "Download") {
		request >> SHA1;
		request >> fileSize;
		request >> serverLocation;

		// Update Priority queue 

		// --F

		return;
	}
	else 
		// Error notification

		// --F

		return;
}

void uploadFile(message& request, message& response, socket& s, json& users, json& files) {
	string fname, username, SHA1, location;
	size_t size;
	int owners = 1; // Increment file owners in one

	request >> username;
	request >> fname;
	request >> SHA1;
	request >> size;

	// Check if the file was uploaded before

	if(files.find(SHA1) != files.end()) {
		// File already exists

		// Update Info 
		users[username]["files"][fname] = SHA1;

		owners = files[SHA1]["owners"];
		owners += 1;
		files[SHA1]["owners"] = owners;

		saveUsersFilesInfo(users, files);

		// Notify user

		// --F

		return;

	}

	// Find server to Upload file, according to Priority queue

	// --F

	cout << "File to be uploaded: " << fname << " of size (bytes): " << size << endl;

	// Send Server info to client, and update priority queue

	// --F

}

void downloadFile(message& request, message& response, socket& s, json& users, json& files) {
	string fname, username, SHA1, location;
	size_t fsize;
	request >> username;
	request >> fname;

	if (users[username]["files"][fname].is_string()) {
		SHA1 = users[username]["files"][fname];

		//Search location of file to download
		if (files[SHA1]["location"].is_string()) {
			location = files[SHA1]["location"];
			fsize = files[SHA1]["size"];	// .get<long long int>();

			// Send Server info to client, and update priority queue

			// --F

			return;
		}
	}
	else {
		// File doesn't found 

		// response << "Error" << 0;
		// s.send(response);

		// --F

		return;


	}

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
		users[username]["files"] = {};
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

void messageHandler(message& request, message& response, socket& s, json& users, json& files) {
	string op;
	request >> op;

	cout << "Option: " << op << endl;

	if(op == "CreateUser") {
		createUser(request, response, s, users);
	} else  if(op == "Login") {
		verifyUser(request, response, s, users);
	} else  if(op == "Upload") {
		uploadFile(request, response, s, users, files);
	} else if(op == "Download") {
		downloadFile(request, response, s, users, files);
	} else if(op == "List_files") {
		listFiles(request, response, s, users);
	} else if(op == "Delete") {
		deleteFile(request, response, s, users, files);
	} else {
		response << "Error";
	}
}

int main() {
	cout << "This is the Broker\n"; 

	context ctx;  
	socket s(ctx, socket_type::rep);

	cout << "Binding socket to tcp port 5555\n";
	s.bind("tcp://*:5555");

	
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

		message request, response;
		s.receive(request);

		cout << "Message received!\n";

		messageHandler(request, response, s, users, files);
	}

	return 0;
}