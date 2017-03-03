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

bool DOWNLOADING=false;
size_t total_downloaded = 0, chunks_downloaded = 0;
long file_size=0;

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

void getFileName(char fname[100], char file_name[100]) {
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
}

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

void CheckFile(socket &download_socket) {
	message check_sum_msg;
	string path, fname, server_identity, empty;
	unsigned char check_sum[SHA_DIGEST_LENGTH];
	unsigned char *received_check_sum = NULL;

	download_socket.receive(check_sum_msg);

	check_sum_msg >> empty >> fname;
	received_check_sum = (unsigned char *)check_sum_msg.raw_data(2);
	cout << "Received Check sum: ";
	printChecksum(received_check_sum);

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

int SaveFile(socket &download_socket) {
	message answer, req_check_sum;
	string path, fname, server_identity, empty;
	size_t size;
	int size_chunk;
	char *data;
	FILE* f;
	double progress;

	download_socket.receive(answer);

	answer >> empty;

	if(answer.get(1) == "Error") {
		if(atoi(answer.get(2).c_str()) == 0) {
			cout << "\nThe file " << fname << " requested does not exist or couldn't be read!!" << endl;
			return -1;
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

	if (total_downloaded == 0) {
		f = fopen(path.c_str(), "wb");
		answer >> file_size;
		answer >> size_chunk;
		size = answer.size(4);
		data = (char*) answer.raw_data(4);
	} else {
		f = fopen(path.c_str(), "ab");
		fseek(f, 0L, SEEK_END);
		answer >> size_chunk;
		size = answer.size(3);
		data = (char*) answer.raw_data(3);
	}

	fwrite(data, 1, size, f);
	total_downloaded += size;
	chunks_downloaded++;
	fclose(f);

	progress = ((double)total_downloaded/(double)file_size);
	if((progress*100.0) <= 100.0){
		printf("\r[%3.2f%%]",progress*100.0);
	}
	fflush(stdout);

	if (size == 0 || (int)size < size_chunk) {
		cout << endl;
		cout << "Chunks received: " << chunks_downloaded << "\nBytes received: " << total_downloaded << endl;
		return 0;
	}
	return 1;
}

void downloadFile(socket &broker_socket, socket &download_socket, string op, string username) {
	//char fname[100];
	string server_address;
	string path, fname;
	message send_request, broker_response, metadata_file, req_check_sum;

	if(DOWNLOADING) {
		cout << "you are already downloading a file, wait untill that ends to request another download!\n";
		return;
	}

	cout << "Enter file name: \n";
	//cin.getline(fname, sizeof(fname));
	//cin.getline(fname, sizeof(fname));
	getline(cin, fname);

	cout << "enter server address: " << endl; //send req to broker
	cin >> server_address; //rec response from broker
	download_socket.connect(server_address); //connect to server
	DOWNLOADING = true;
	bool error = false;
	cout << "Saving to file..." << endl;

	metadata_file << "" << op << username << fname << total_downloaded;
	while(true) {
		download_socket.send(metadata_file);
		int stat = SaveFile(download_socket);
		if(stat == 1) {
			metadata_file << "" << op << username << fname << total_downloaded;
		} else if(stat == 0) {
			break;
		} else if(stat == -1) {
			error = true;
			break;
		}
	}

	if((int)total_downloaded == file_size && !error) {
		req_check_sum << "" << op << username << fname << total_downloaded;
		download_socket.send(req_check_sum);
		CheckFile(download_socket);
		//cout << "total: " << total_downloaded << "; chunks: " << chunks_downloaded << "; file size: " << file_size << "; downloading: " << DOWNLOADING << endl;
	}

	total_downloaded = 0;
	chunks_downloaded = 0;
	file_size=0;
	DOWNLOADING = false;
	download_socket.disconnect(server_address);
}

void uploadFile(socket &broker_socket, socket &download_socket, string op, string username) {
	FILE* f;
	char *data;
	size_t size, offset;
	char file_name[100];//, fname[100];
	message file_message, server_answer, check_sum_msg;
	double progress;
	string empty, server_address, fname;

	unsigned char check_sum[SHA_DIGEST_LENGTH];

	cout << "Enter file name: \n";
	//cin.getline(fname, sizeof(fname));
	//cin.getline(fname, sizeof(fname));
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

void messageHandler(message &server_response, socket &s) {
	string op, empty;
	server_response >> empty >> op;

	if(op == "Upload") {
		ReadFile(server_response, s);
	} else if(op == "Download") {
		SaveFile(server_response, s);
	} else {
		cout << "Message unknown: " << op << endl;
	}
}

int main() {

	message login, response, create_user;
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
						p.add(s, poller::poll_out);
						cin.ignore(numeric_limits<streamsize>::max(), '\n');
					} else if(op == "Download" || op == "download" || op == "down") {
						downloadFile(broker_socket, s, "Download", username);
						p.add(s, poller::poll_in);
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
