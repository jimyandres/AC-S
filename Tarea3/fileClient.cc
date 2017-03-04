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

//bool DOWNLOADING=false;
/*size_t total_downloaded = 0, chunks_downloaded = 0;
long file_size=0;*/

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

/*void getFileName(char fname[100], char file_name[100]) {
	char *temp_pt = strrchr(fname, '/');
	if(temp_pt == NULL) {
		strcpy (file_name, fname);
	} else {
		//cout << strlen(fname) << endl;
		size_t pos = temp_pt-fname+1, j=0;
		while(pos < strlen(fname)) {
			file_name[j] = fname[pos];
			pos++;
			j++;
		}
		file_name[j] = '\0';
	}
	//cout << "file: " << file_name << endl;
}*/

void getFileName(string fname, char file_name[100]) {
	char * tmp = new char[fname.size()+1];
	memcpy(tmp, fname.c_str(), fname.size()+1);
	char *temp_pt = strrchr(tmp, '/');
	if(temp_pt == NULL) {
		strcpy (file_name, tmp);
	} else {
		//cout << strlen(fname) << endl;
		size_t pos = temp_pt-tmp+1;
		strcpy(file_name, &tmp[pos]);
	}
	delete [] tmp;
	//cout << "file: " << file_name << endl;
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
	message check_sum_msg;
	//string empty;
	unsigned char check_sum[SHA_DIGEST_LENGTH];
	unsigned char *received_check_sum = NULL;

	//download_socket.receive(check_sum_msg);

	//check_sum_msg >> empty >> fname;

	received_check_sum = (unsigned char *)server_response.raw_data(5);
	cout << "Received Check sum: ";
	printChecksum(received_check_sum);

	/*struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;

	path = (string)homedir + "/Descargas/Downloads/";

	struct stat sb;
	lstat(path.c_str(), &sb);

	if(!S_ISDIR(sb.st_mode)) {
		string url = "mkdir -p " + (string)path;
		const char * directory =  url.c_str();
		system(directory);
	}

	path.append(fname);*/

	getCheckSum(path, (unsigned char *)&check_sum);

	if(memcmp(check_sum, received_check_sum, SHA_DIGEST_LENGTH)) {
		cout << "\nThe Checksum failed, please try again!!" << endl;
		if( remove(path.c_str()) != 0 )
			cout << "Error deleting file: " << path << endl;
		else
			cout << "File: " << path << " successfully deleted\n";
	} else {
		cout << "File saved successfully!!\n";
	}
}

void ReadFile(message &answer, socket &upload_socket, string username) {

}

void SaveFile(message &answer, socket &download_socket, string username) {
	message req_check_sum, metadata_file;
	string path, fname, empty, server_address = "";
	size_t size, offset;
	int size_chunk;
	char *data;
	FILE* f;
	double progress;
	long file_size;

	if(answer.get(2) == "Error") {
		if(atoi(answer.get(3).c_str()) == 0) {
			cout << "\nThe file " << fname << " requested does not exist or couldn't be read!!" << endl;
			return;
		}
	}

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
		//req_check_sum << "" << "Download" << username << ;
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

	progress = ((double)offset/(double)file_size);
	if((progress*100.0) <= 100.0){
		printf("\r[%3.2f%%]",progress*100.0);
	}
	fflush(stdout);

	if (size == 0 || (int)size < size_chunk) {
		cout << endl;
		cout << "Bytes received: " << offset << endl;
		req_check_sum << "" << "Download" << username << fname << offset;
		download_socket.send(req_check_sum);
	} else {
		metadata_file << "" << "Download" << username << fname << offset;
		download_socket.send(metadata_file);
	}
}

void downloadFile(socket &broker_socket, socket &download_socket, string op, string username) {
	string server_address;
	string path, fname;
	message send_request, broker_response, metadata_file, req_check_sum;

	cout << "Enter file name: \n";
	getline(cin, fname);

	cout << "enter server address: " << endl; //send req to broker
	cin >> server_address; //rec response from broker
	download_socket.connect(server_address); //connect to server

	cout << "Saving to file..." << endl;
	size_t offset = 0;

	metadata_file << "" << op << username << fname << offset;
	if(download_socket.send(metadata_file)) {
		cout << "request sended" << endl;
	} else {
		cout << "request failed" << endl;
	}
}

void uploadFile(socket &broker_socket, socket &download_socket, string op, string username) {
	FILE* f;
	char *data;
	size_t size, offset;
	char file_name[100];
	message file_message, server_answer, check_sum_msg;
	double progress;
	string empty, server_address, fname;

	unsigned char check_sum[SHA_DIGEST_LENGTH];

	cout << "Enter file name: \n";
	getline(cin, fname);

	cout << "enter server address: " << endl; //send req to broker
	cin >> server_address; //rec response from broker
	download_socket.connect(server_address); //connect to server

	getFileName(fname, file_name);

	if(!(f = fopen(fname.c_str(), "rb"))) {
		cout << "\nThe file " << file_name << " requested does not exist or couldn't be read!!" << endl;
		return;
	}
	fflush(f);

	fseek(f, 0L, SEEK_END);
	long sz = ftell(f);
	cout << "\nFile size (bytes): " << sz << endl;
	fseek(f, 0L, SEEK_SET);

	cout << "File to upload: " << file_name << endl;
	SHA_CTX sha_ctx;
	SHA1_Init(&sha_ctx);

	offset = 0;
	while(true) {
		fseek(f, offset, SEEK_SET);
		data = (char*) malloc (sizeof(char)*CHUNK_SIZE);
		assert(data);

		size = fread(data, 1, CHUNK_SIZE, f);
		SHA1_Update(&sha_ctx, data, size);

		file_message << "" << op << username << file_name << CHUNK_SIZE << offset;

		file_message.push_back(data, size);
		free(data);

		download_socket.send(file_message);

		download_socket.receive(server_answer);

		server_answer >> empty;

		offset += size;
		progress = ((double)offset/(double)sz);
		if((progress*100.0) <= 100.0){
			printf("\r[%3.2f%%]",progress*100.0);
		}
		fflush(stdout);

		if(server_answer.get(1) == "Error") {
			if(atoi(server_answer.get(2).c_str()) == 0) {
			cout << "\nThe file requested does not exist or couldn't be read!!" << endl;
			return;
			}
		} else if(server_answer.get(1) == "Done") {
			fclose(f);
			break;
		}
	}

	SHA1_Final(check_sum, &sha_ctx);

    cout << "\nCalculated check sum: ";
	printChecksum(check_sum);

	check_sum_msg << "" << op << username << file_name << "Check";

    check_sum_msg.push_back(check_sum, SHA_DIGEST_LENGTH);

    download_socket.send(check_sum_msg);

	download_socket.receive(server_answer);

	server_answer >> empty;

	if(server_answer.get(1) == "Error") {
		if(atoi(server_answer.get(2).c_str()) == 0) {
			cout << "\nThe file requested does not exist or couldn't be read!!" << endl;
			return;
		} else if(atoi(server_answer.get(2).c_str()) == 1) {
			cout << "\nThe Checksum failed, please try again!!" << endl;
		}
	}

	if(server_answer.get(1) == "ok") {
		cout << "File has been uploaded successfully!!" << endl;
	}
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
	string status;
	char filename[100];

	cout << "Enter File name: \n";
	cin.getline(filename, sizeof(filename));
	cin.getline(filename, sizeof(filename));

	request << op << username << filename;
	s.send(request);
	s.receive(response);

	response >> status;
	cout << "Deleting file \"" << filename << "\": " << status << endl;
}

void messageHandler(message &server_response, socket &s, string username) {
	string op, empty, address;
	server_response >> empty >> op;

	if(op == "Upload") {
		ReadFile(server_response, s, username);
	} else if(op == "Download") {
		SaveFile(server_response, s, username);
	} else if(op == "Disconnect") {
		server_response >> address;
		s.disconnect(address);
	}
	else {
		cout << "Message unknown: " << op << endl;
	}
}

int main() {

	message login, response, create_user, server_response;
	string op, create, answer;
	char username[40];
	string password = "";
	int access;

	cout << "This is the client\n";

	context ctx;
	socket broker_socket(ctx, socket_type::req);
	socket s(ctx, socket_type::dealer);

	int standardin = fileno(stdin);
	poller p;

	p.add(broker_socket, poller::poll_in);
	p.add(standardin, poller::poll_in);
	p.add(s, poller::poll_in);
	//p.add(s, poller::poll_out);

	cout << "Connecting to tcp port 5555\n";
	broker_socket.connect("tcp://localhost:5555");

	while(true) {
		cout << "Create new account (yes/no): ";
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
	response >> access;

	if (access) {
		printMenu();
		cout <<  "\nEnter action to perform: " << endl;
		while(true) {
			//cin >> op;
			if(p.poll()) {
				if(p.has_input(broker_socket)) {
					//do something eith message
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
					if(s.receive(server_response)) {
						cout << "response received" << endl;
					} else {
						cout << "failed to retrieve response" << endl;
					}
					messageHandler(server_response, s, username);
				}
				/*if(p.has_output(s)){
					s.receive(server_response);
					messageHandler(server_response, s, username);
				}*/
			}
		}
	}
	else cout << "\nUser not found!!\n\n";

	cout << "Finished\n";
	broker_socket.close();
	s.close();
	ctx.terminate();
	return 0;
}
