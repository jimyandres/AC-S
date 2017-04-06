#include <iostream>
#include <stdio.h>
#include <string.h>
#include <zmqpp/zmqpp.hpp>
#include <sys/stat.h>

#include <openssl/sha.h>
#include "src/json.hpp"

#include <unistd.h> 		//Get the user id of the current user
#include <sys/types.h>
#include <pwd.h>			//Get the password entry (which includes the home directory) of the user

#define CHUNK_SIZE 5242880

using namespace std;
using namespace zmqpp;
using json = nlohmann::json;

void printChecksum (unsigned char * check_sum) {
	char mdString[SHA_DIGEST_LENGTH*2+1];
 
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
         sprintf(&mdString[i*2], "%02x", (unsigned int)check_sum[i]);
 
    printf("%s\n", mdString);
}

void ChecksumToString(unsigned char * check_sum, char mdString[SHA_DIGEST_LENGTH*2+1]) {
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)check_sum[i]);
}

// string appendExtension(char file_name[SHA_DIGEST_LENGTH*2+1], char extension[5]) {
// 	string ans;
// 	ans.insert(0, file_name, SHA_DIGEST_LENGTH*2);
// 	ans.append(extension);
// 	return ans;
// }

void printMenu() {
	cout << "\n\n***********************************\n";
	cout << "Enter action to perform: \n\tList_files\n\tDownload\n\tUpload\n\tDelete\n\tExit\n";
	cout << "\n";
}

void disconnectFromServer(socket &s, string address) {
	cout << "Disconnected from " << address << endl;
	s.disconnect(address);
}

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

void getCheckSum(string path, unsigned char *check_sum) {
	FILE* f;
	char *data;
	SHA_CTX sha_ctx;

	if(!(f = fopen(path.c_str(), "rb"))) {
		memset(check_sum, '0', SHA_DIGEST_LENGTH);
		return;
	}

	SHA1_Init(&sha_ctx);
	size_t size;
	while(true) {
		data = (char*) malloc (sizeof(char)*CHUNK_SIZE);
		size = fread(data, 1, CHUNK_SIZE, f);
		SHA1_Update(&sha_ctx, data, size);
		if(size == 0 || size < CHUNK_SIZE)
			break;
		free(data);
	}
	fclose(f);
	SHA1_Final(check_sum, &sha_ctx);
}

void CheckFile(message &server_response, string path) {
	unsigned char check_sum[SHA_DIGEST_LENGTH];
	unsigned char *received_check_sum = NULL;

	received_check_sum = (unsigned char *)server_response.raw_data(6);
	cout << "Received Check sum: ";
	printChecksum(received_check_sum);

	getCheckSum(path, (unsigned char *)&check_sum);

	cout << "Calculated check sum: ";
	printChecksum(check_sum);

	if(memcmp(check_sum, received_check_sum, SHA_DIGEST_LENGTH)) {
		cout << "\nThe Checksum failed, please try again!!" << endl;
		if( remove(path.c_str()) != 0 )
			cout << "Error deleting file: " << path << endl;
		else
			cout << "File: \"" << path << "\" successfully deleted\n";
	} else {
		cout << "File \"" << path << "\" saved successfully!!\n";
	}
}

void ReadFile(message &answer, socket &upload_socket, string username) {
	FILE* f;
	string path, fname, file_size_str, offset_str;
	size_t offset, size;
	long file_size;
	char *data;
	unsigned char check_sum[SHA_DIGEST_LENGTH];
	message file_message;

	answer >> path;
	answer >> fname;
	answer >> offset_str;
	offset = atol(offset_str.c_str());
	answer >> file_size_str;
	file_message << "" << "Upload" << username << path << fname << file_size_str << offset_str;
	file_size = atol(file_size_str.c_str());

	if((int)offset == file_size) {
		getCheckSum(path, (unsigned char *)&check_sum);
		file_message.push_back(check_sum, SHA_DIGEST_LENGTH);
		upload_socket.send(file_message);
		return;
	}

	if(!(f = fopen(path.c_str(), "rb"))) {
		cout << "\nThe file " << path << " requested does not exist or couldn't be read!!" << endl;
		return;
	}
	fflush(f);
	fseek(f, offset, SEEK_SET);
	data = (char*) malloc (sizeof(char)*CHUNK_SIZE);
	size = fread(data, 1, CHUNK_SIZE, f);
	
	file_message << CHUNK_SIZE;
	file_message.push_back(data, size);
	free(data);
	fclose(f);
	upload_socket.send(file_message);
}

void SaveFile(message &answer, socket &download_socket, string username) {
	message req_check_sum, metadata_file;
	string path, fname, server_address, server_fname, file_size_str, offset_str, nPart;
	size_t size, offset;
	int size_chunk;
	char *data;
	FILE* f;
	long file_size;

	answer >> fname;
	struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;
	path = (string)homedir + "/Descargas/Downloads/";
	struct stat sb;
	lstat(path.c_str(), &sb);

	if(!S_ISDIR(sb.st_mode)) {
		string url = "mkdir -p " + (string)path;
		const char * directory =  url.c_str();
		system(directory);
	}
	path.append(fname);

	answer >> server_fname;
	// answer >> offset_str;
	// offset = atol(offset_str.c_str());
	answer >> nPart;
	answer >> file_size_str;
	file_size = atol(file_size_str.c_str());

	if (nPart == "0") {
		f = fopen(path.c_str(), "wb");
	} 
	// else if((int)offset == file_size) {
	// 	CheckFile(answer, path);
	// 	server_address = answer.get(7);
	// 	disconnectFromServer(download_socket, server_address);
	// 	return;
	// } 
	else {
		f = fopen(path.c_str(), "ab");
		fseek(f, 0L, SEEK_END);
	}

	answer >> size_chunk;
	size = answer.size(7);
	data = (char*) answer.raw_data(7);

	fwrite(data, 1, size, f);
	// offset += size;
	fclose(f);
	// offset_str = to_string(offset);

	/*progress = ((double)offset/(double)file_size);
	if((progress*100.0) <= 100.0){
		printf("\r[%3.2f%%]",progress*100.0);
	}
	fflush(stdout);*/

	if (size == 0 || (int)size < size_chunk) {
		cout << endl;
		cout << "Bytes received: " << offset << endl;
		// req_check_sum << "" << "Download" << username << fname << server_fname << offset_str << file_size_str;
		// download_socket.send(req_check_sum);
	} 
	// else {
	// 	metadata_file << "" << "Download" << username << fname << server_fname << offset_str << file_size_str;
	// 	download_socket.send(metadata_file);
	// }
}

void downloadFile(socket &broker_socket, socket &download_socket, string op, string username) {
	string servers_address;
	string server_fname, fname, file_size_str;
	message request_broker, response_broker, metadata_file, server_response;
	size_t offset = 0;
	string offset_str = to_string((int)offset);

	cout << "Enter file name: \n";
	getline(cin, fname);

	if(fname == "cancel" || fname == "q") {
		cout << "Canceled action!!" << endl;
		return;
	}

	request_broker << "Download" << username << fname;
	broker_socket.send(request_broker);
	broker_socket.receive(response_broker);

	json locations;

	if(response_broker.get(0) == "Error") {
		servers_address = response_broker.get(1);
		cout << "Error: " <<  servers_address << endl;
		return;
	} else {
		response_broker >> servers_address;
		locations = json::parse(servers_address);
		response_broker >> file_size_str;
		response_broker >> server_fname;
	}
	
	int count = locations.size();
	int num=0;
	for (int i = 0; i < count; ++i) {
		cout << "Connected to: " << locations[i] << endl;
		download_socket.connect(locations[i]); //connect to server
		cout << "Saving part " << i << "..." << endl;
		string part = to_string((int)num);
		metadata_file << "" << op << fname << server_fname << part << file_size_str ;
		download_socket.send(metadata_file);

		download_socket.receive(server_response);
		server_response >> op; // empty
		server_response >> op;
		if (op == "Download")
			SaveFile(server_response, download_socket, username);
		else if (op == "Error") {
			string type;
			server_response >> type;
			cout << "Error downloading file" << endl;
			cout << type;
			break;
		} else {
			cout << "Error downloading file";
			break;
		}
		num++;
	}

	// cout << "Connected to: " << servers_address << endl;
	// download_socket.connect(server_address); //connect to server

	// cout << "Saving..." << endl;
	// metadata_file << "" << op << username << fname << server_fname << offset_str << file_size_str;
	// download_socket.send(metadata_file);
}

void uploadFile(socket &broker_socket, socket &upload_socket, string op, string username) {
	FILE* f;
	char *data;
	size_t size, offset;
	char file_name[100], file_SHA1[SHA_DIGEST_LENGTH*2+1];
	message file_message, server_answer, check_sum_msg, request_broker, response_broker;
	string empty, servers_address, fname, server_fname, file_size_str, offset_str;
	//char extension[] = ".dat";

	json locations;

	cout << "Enter file name: " << endl;
	getline(cin, fname);

	if(fname == "cancel" || fname == "q") {
		cout << "Canceled action!!" << endl;
		return;
	}

	getFileName(fname, file_name);

	if(!(f = fopen(fname.c_str(), "rb"))) {
		cout << "\nThe file " << file_name << " requested does not exist or couldn't be read!!" << endl;
		return;
	}
	fflush(f);

	fseek(f, 0L, SEEK_END);
	long sz = ftell(f);
	file_size_str = to_string(sz);
	cout << "\nFile size (bytes): " << file_size_str << endl;
	fseek(f, 0L, SEEK_SET);

	unsigned char check_sum[SHA_DIGEST_LENGTH];
	getCheckSum(fname, (unsigned char *)&check_sum);
	ChecksumToString((unsigned char *)&check_sum, file_SHA1);

	request_broker << "Upload" << username << file_name << file_SHA1 << file_size_str;
	broker_socket.send(request_broker);
	broker_socket.receive(response_broker);


	if(response_broker.get(0) == "Done") {
		cout << "\"" << file_name << "\" successfully uploaded!!" << endl;
		return;
	} else if(response_broker.get(0) == "Error") {
		servers_address = response_broker.get(1);

		locations = json::parse(servers_address);

		cout << "Error: " <<  locations[0] << endl;
		return;
	} else {
		response_broker >> servers_address;
		locations = json::parse(servers_address);
	}

	// offset = 0;

	for (int i = 0; i < (int)locations.size(); ++i)
	{
		cout << "Connected to: " << locations[i] << endl;
		upload_socket.connect(locations[i]); //connect to server

		file_size_str = to_string(CHUNK_SIZE);
		
		// offset_str =  to_string((int)offset);
		cout << "File to upload: " << file_name;
		cout << "\tPart to upload: " << i << "\n";

		data = (char*) malloc (sizeof(char)*CHUNK_SIZE);
		size = fread(data, 1, CHUNK_SIZE, f);
		string file_SHA1_str(file_SHA1);
		file_message << "" << op << username << fname << file_SHA1_str+"."+to_string(i) << to_string(size) << /*offset_str <<*/ CHUNK_SIZE;
		file_message.push_back(data, size);
		free(data);

		upload_socket.send(file_message);

		// offset += CHUNK_SIZE;
	}

	// upload_socket.connect(locations[0]); //connect to server
	// offset = 0;
	// offset_str = to_string((int)offset);
	// cout << "File to upload: " << file_name << "\n";
	// file_message << "" << op << username << fname << file_SHA1 << file_size_str << offset_str << CHUNK_SIZE;

	// data = (char*) malloc (sizeof(char)*CHUNK_SIZE);
	// size = fread(data, 1, CHUNK_SIZE, f);
	// file_message.push_back(data, size);
	// free(data);

	// upload_socket.send(file_message);
}

void listFiles(socket &s, string op, string username) {
	message request, response;
	string files;

	request << op << username;
	s.send(request);
	cout << "Files:" << endl;
	s.receive(response);
	response >> files;
	cout << files << endl;
}

void deleteFile(socket &s, string op, string username, socket &server) {
	message request, response, server_req;
	string status, filename, server_address, server_fname, file_size, ans;

	cout << "Enter File name: \n";
	getline(cin, filename);
	if(filename == "cancel" || filename == "q") {
		cout << "Canceled action!!" << endl;
		return;
	}

	request << op << username << filename;
	s.send(request);
	s.receive(response);
	if(response.get(0) == "Done") {
		status = response.get(1);
		cout << "Deleting file \"" << filename << "\": " << status << endl;
		return;
	} else if(response.get(0) == "Error") {
		ans = response.get(1);
		cout << "Error: " << ans << endl;
		return;
	}
	response >> server_fname >> file_size >> server_address;
	cout << "Connected to: " << server_address << endl;
	server.connect(server_address);
	server_req << "" << "Delete" << server_fname << file_size;
	server.send(server_req);
}

void messageHandler(message &server_response, socket &s, string username) {
	string op, empty, address, msg, path, server_address;
	server_response >> empty >> op;
	message disconnect;

	if(op == "Upload") {
		ReadFile(server_response, s, username);
	} else if(op == "Download") {
		SaveFile(server_response, s, username);
	} else if(op == "Error") {
		server_response >> msg;
		cout << "Error: " << msg << endl;
		server_response >> server_address;
		disconnectFromServer(s, server_address);
	} else if(op == "UpDone") {
		char file_name[100];
		server_response >> path;
		getFileName(path, file_name);
		cout << path << endl;
		string name(file_name);
		cout << "\"" << path.substr(0,40) << "\" successfully uploaded!!" << endl;
		server_response >> server_address;
		disconnectFromServer(s, server_address);
	} else if(op == "DeleteDone") {
		server_response >> msg;
		cout << "Status: " << msg << endl;
		server_response >> server_address;
		disconnectFromServer(s, server_address);
	} else {
		cout << "Message unknown: " << op << endl;
	}
}

int main(int argc, char* argv[]) {
	message login, response, create_user, server_response;
	string op, create, answer;
	char username[40];
	string password = "";
	int access;
	string broker_address = "tcp://";

	cout << "This is the client\n";

	context ctx;
	socket broker_socket(ctx, socket_type::req);
	socket s(ctx, socket_type::dealer);

	int standardin = fileno(stdin);
	poller p;

	p.add(broker_socket, poller::poll_in);
	p.add(standardin, poller::poll_in);
	p.add(s, poller::poll_in);

	if (argc != 2) {
		cout << "Please use like this: ./fileClient address:port\n";
		return EXIT_FAILURE;
	} else {
		broker_address.append(argv[1]);
	}

	cout << "Connecting to tcp port " << broker_address << endl;
	broker_socket.connect(broker_address);

	while(true) {
		cout << "\nCreate new account (yes/no): ";
		cin >> create;

		if (create == "yes" || create == "y") {
			cout << "Enter User name: \n";
			cin >> username;

			cout << "Enter Password: \n";
			cin >> password;

	        create_user << "CreateUser" << username << password;
	        broker_socket.send(create_user);
	        broker_socket.receive(response);

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

	    } else cout << "Invalid option!" << endl;
	}
	cin.ignore(numeric_limits<streamsize>::max(), '\n');

	login << "Login" << username << password;

	broker_socket.send(login);
	broker_socket.receive(response);

	if(response.get(0) == "Failed") {
		answer = response.get(1);
		cout << answer;
	} else {
		response >> access;
		if (access) {
			printMenu();
			cout <<  "\nEnter action to perform: " << endl;
			while(true) {
				if(p.poll()) {
					if(p.has_input(s)){
						s.receive(server_response);
						messageHandler(server_response, s, username);
					}
					if(p.has_input(broker_socket)) {
						cout <<"Message" << endl;
					}
					if(p.has_input(standardin)) {
						getline(cin, op);
						if(op == "Upload" || op == "upload" || op == "up") {
							uploadFile(broker_socket, s, "Upload", username);
							// cin.ignore(numeric_limits<streamsize>::max(), '\n');
						} else if(op == "Download" || op == "download" || op == "down") {
							downloadFile(broker_socket, s, "Download", username);
						} else if(op == "List_files" || op == "list_files" || op == "ls") {
							listFiles(broker_socket, "List_files", username);
						} else if(op == "Delete" || op == "delete" || op == "del") {
							deleteFile(broker_socket, "Delete", username, s);
						} else if(op == "Exit" || op == "exit" || op == "ex") {
							break;
						} else {
							cout << "\nInvalid option, please enter one of the listed options!" << endl;
						}
					}
				}
			}
		} else
			cout << "\nUser not found!!\n\n";
	}
	cout << "\nFinished\n";
	broker_socket.close();
	s.close();
	ctx.terminate();
	return 0;
}
