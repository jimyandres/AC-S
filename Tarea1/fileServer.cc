#include <iostream>   
#include <string>
#include <fstream>
#include <zmqpp/zmqpp.hpp>
#include <dirent.h>
//#include <stdio.h>

#include <jsoncpp/json/json.h>
#include <jsoncpp/json/writer.h>

using namespace std;
using namespace zmqpp;

void listFiles(message &m, message &response, socket &s) {
	string files, user;

	m >> user;
	
	DIR *dir;
	struct dirent *ent;
	const char * url =  user.c_str();
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
	response << files;
	s.send(response);
	
}

void deleteFile(message &m, message &response, socket &s) {
	string user, filename, l;

	m >> user;
	m >> filename;

	l = user + "/" + filename;

	cout << "Deleting: " + l;
	const char * location =  l.c_str();

	if( remove( location ) != 0 ){
	    response << "Error deleting file";
	}
	else {
		response << "File successfully deleted";
	}
	s.send(response);
}

void uploadFile(message &m, message &response) {
	string fname;
	m >> fname;
	/*if(!(f = fopen("testdata", "r"))) {
		cout << "No se pudo crear el arhcivo" << endl;
		return;
	}*/
	cout << "File to be uploaded: " << fname << endl;
	response << "ok";
}

void downloadFile(message &client_request, message &server_response, socket &s, bool ready_flag = false) {

	FILE* f;
	char *data;
	size_t size;

	string fname;
	client_request >> fname;

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
	fclose(f);
}

void createUser(message &m, message &response, socket &s) {
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
    if (root[user]["password"] == password) {
    	response << "Failed" << "Username \"" + user + "\" already exists!";
    }
    else {
    	root[user]["password"] = password;
    	test.close();
    	ofstream test1("users.json");
    	cout << "New user created: " << user << endl;
    	writer.write(test1,root);

    	url = "mkdir -p " + user;
    	const char * directory =  url.c_str();
    	system(directory);
    	response << "Ok" << "Username \"" + user + "\" was created!";
    }

    s.send(response);

}

void verifyUser(message &m, message &response, socket &s) {

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
    	response << 1;
    }
    else response << 0;

    s.send(response);

    //return 0;

    /*std::cout << root << std::endl;
    root[user]["password"] = "buffalo";
    test.close();
    std::ofstream test1("users.json");
    writer.write(test1,root);
    std::cout << root << std::endl;
    return 1;*/

}

void messageHandler(message &client_request, message &server_response, socket &s) {
	string op;
	client_request >> op;

	cout << "Option: " << op << endl;

	if(op == "CreateUser") {
		createUser(client_request, server_response, s);
	} else  if(op == "Login") {
		verifyUser(client_request, server_response, s);
	} else  if(op == "Upload" || op == "upload") {
		uploadFile(client_request, server_response);
	} else if(op == "Download" || op == "download") {
		downloadFile(client_request, server_response, s);
	} else if(op == "FileData") {
		downloadFile(client_request, server_response, s, true);
	} else if(op == "List_files" || op == "list_files") {
		listFiles(client_request, server_response, s);
	} else if(op == "Delete" || op == "delete") {
		deleteFile(client_request, server_response, s);
	} else {
		server_response << "Error";
	}
}

int main() {
	cout << "This is the server\n";  // imprime en pantalla

	context ctx;  //declaramos una variable ctx como un tipo de contexto
	socket s(ctx, socket_type::rep); //declaramos un socket de tipo s, necesita tener un contexto

	cout << "Binding socket to tcp port 5555\n";
	s.bind("tcp://*:5555");

	while(true) {
		cout << "Waiting for message to arrive!\n";

		message client_request, server_response;
		s.receive(client_request);

		cout << "Message received!\n";

		messageHandler(client_request, server_response, s);
		//s.send(response);
	}

	return 0;
}