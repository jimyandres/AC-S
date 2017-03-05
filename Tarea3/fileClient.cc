#include <iostream>
#include <stdio.h>
#include <string.h>
#include <zmqpp/zmqpp.hpp>
#include <sys/stat.h>

#include <openssl/sha.h>

#include <unistd.h> 		//Get the user id of the current user
#include <sys/types.h>
#include <pwd.h>			//Get the password entry (which includes the home directory) of the user

#define CHUNK_SIZE 5242880

using namespace std;
using namespace zmqpp;

void printChecksum (unsigned char * check_sum) {
	char mdString[SHA_DIGEST_LENGTH*2+1];
 
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
         sprintf(&mdString[i*2], "%02x", (unsigned int)check_sum[i]);
 
    printf("%s\n", mdString);
}

void printMenu() {
	cout << "\n\n***********************************\n";
	cout << "Enter action to perform: \n\tList_files\n\tDownload\n\tUpload\n\tDelete\n\tExit\n";
	cout << "\n";
}

void disconnectFromServer(socket &s, string address) {
	cout << "Disconnected from " << address << endl;
	//s.disconnect(address);
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
	cout << "Calculated check sum: ";
	printChecksum(check_sum);
}

void CheckFile(message &server_response, string path) {
	unsigned char check_sum[SHA_DIGEST_LENGTH];
	unsigned char *received_check_sum = NULL;

	received_check_sum = (unsigned char *)server_response.raw_data(5);
	cout << "Received Check sum: ";
	printChecksum(received_check_sum);

	getCheckSum(path, (unsigned char *)&check_sum);

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
	string path, fname;
	size_t offset, size;
	long file_size;
	char *data;
	unsigned char check_sum[SHA_DIGEST_LENGTH];
	message file_message;

	answer >> path;
	answer >> fname;
	answer >> offset;
	answer >> file_size;
	file_message << "" << "Upload" << username << path << fname << file_size << offset;

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
	upload_socket.send(file_message);
}

void SaveFile(message &answer, socket &download_socket, string username) {
	message req_check_sum, metadata_file;
	string path, fname, server_address;
	size_t size, offset;
	int size_chunk;
	char *data;
	FILE* f;
	//double progress;
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

	answer >> offset;
	answer >> file_size;

	if (offset == 0) {
		f = fopen(path.c_str(), "wb");
	} else if((int)offset == file_size) {
		CheckFile(answer, path);
		server_address = answer.get(6);
		disconnectFromServer(download_socket, server_address);
		return;
	} else {
		f = fopen(path.c_str(), "ab");
		fseek(f, 0L, SEEK_END);
	}

	answer >> size_chunk;
	size = answer.size(6);
	data = (char*) answer.raw_data(6);

	fwrite(data, 1, size, f);
	offset += size;
	fclose(f);

	/*progress = ((double)offset/(double)file_size);
	if((progress*100.0) <= 100.0){
		printf("\r[%3.2f%%]",progress*100.0);
	}
	fflush(stdout);*/

	if (size == 0 || (int)size < size_chunk) {
		cout << endl;
		cout << "Bytes received: " << offset << endl;
		req_check_sum << "" << "Download" << username << fname << offset << file_size;
		download_socket.send(req_check_sum);
	} else {
		metadata_file << "" << "Download" << username << fname << offset << file_size;
		download_socket.send(metadata_file);
	}
}

void downloadFile(socket &broker_socket, socket &download_socket, string op, string username) {
	string server_address;
	string path, fname;
	message send_request, broker_response, metadata_file, req_check_sum;

	cout << "Enter file name: \n";
	getline(cin, fname);

	if(fname == "cancel" || fname == "q") {
		cout << "Canceled action!!" << endl;
		return;
	}

	cout << "Enter server address: " << endl; //send req to broker
	cin >> server_address; //rec response from broker
	download_socket.connect(server_address); //connect to server

	size_t offset = 0;
	long file_size = -1;

	cout << "Saving..." << endl;
	metadata_file << "" << op << username << fname << offset << file_size;
	download_socket.send(metadata_file);
}

void uploadFile(socket &broker_socket, socket &upload_socket, string op, string username) {
	FILE* f;
	char *data;
	size_t size, offset;
	char file_name[100];
	message file_message, server_answer, check_sum_msg;
	string empty, server_address, fname;

	cout << "Enter file name: \n";
	getline(cin, fname);

	if(fname == "cancel" || fname == "q") {
		cout << "Canceled action!!" << endl;
		return;
	}

	cout << "enter server address: " << endl; //send req to broker
	cin >> server_address; //rec response from broker

	getFileName(fname, file_name);

	if(!(f = fopen(fname.c_str(), "rb"))) {
		cout << "\nThe file " << file_name << " requested does not exist or couldn't be read!!" << endl;
		return;
	}
	fflush(f);
	upload_socket.connect(server_address); //connect to server

	fseek(f, 0L, SEEK_END);
	long sz = ftell(f);
	cout << "\nFile size (bytes): " << sz << endl;
	fseek(f, 0L, SEEK_SET);

	offset = 0;

	cout << "File to upload: " << file_name << endl;
	file_message << "" << op << username << fname << file_name << sz << offset << CHUNK_SIZE;

	data = (char*) malloc (sizeof(char)*CHUNK_SIZE);
	size = fread(data, 1, CHUNK_SIZE, f);
	file_message.push_back(data, size);
	free(data);

	upload_socket.send(file_message);
}

void listFiles(socket &s, string op, string username) {
	message request, response;
	string files;

	request << op << username;
	s.send(request);
	cout << "Files:" << endl;
	s.receive(response);
	response >> files;
	cout << files;
}

void deleteFile(socket &s, string op, string username) {
	message request, response;
	string status, filename;

	cout << "Enter File name: \n";
	getline(cin, filename);
	if(filename == "cancel" || filename == "q") {
		cout << "Canceled action!!" << endl;
		return;
	}

	request << op << username << filename;
	s.send(request);
	s.receive(response);

	response >> status;
	cout << "Deleting file \"" << filename << "\": " << status << endl;
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
		cout << "\"" << file_name << "\" successfully uploaded!!" << endl;
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

	    } else cout << "Invalid option!\n";
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
					if(p.has_input(broker_socket)) {
						cout <<"Message" << endl;
					}
					if(p.has_input(standardin)) {
						getline(cin, op);
						if(op == "Upload" || op == "upload" || op == "up") {
							uploadFile(broker_socket, s, "Upload", username);
							cin.ignore(numeric_limits<streamsize>::max(), '\n');
						} else if(op == "Download" || op == "download" || op == "down") {
							downloadFile(broker_socket, s, "Download", username);
							cin.ignore(numeric_limits<streamsize>::max(), '\n');
						} else if(op == "List_files" || op == "list_files" || op == "ls") {
							listFiles(broker_socket, "List_files", username);
						} else if(op == "Delete" || op == "delete" || op == "del") {
							deleteFile(broker_socket, "Delete", username);
						} else if(op == "Exit" || op == "exit" || op == "ex") {
							break;
						} else {
							cout << "\nInvalid option, please enter one of the listed options!\n";
						}
					}
					if(p.has_input(s)){
						s.receive(server_response);
						messageHandler(server_response, s, username);
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
