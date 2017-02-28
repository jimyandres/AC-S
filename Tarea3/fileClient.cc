#include <iostream>
#include <stdio.h>
#include <string>
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
	char mdString[33];
 
    for(int i = 0; i < 16; i++)
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

void downloadFile(socket &s, string op, string username) {
	char fname[100];
	string path;
	message metadata_file, answer, req_check_sum;
	size_t size;
	char *data_sha1 = NULL;
	FILE* f;
	int size_chunk;
	long file_size;
	double progress;

	size_t total = 0;
	size_t chunks = 0;

	unsigned char check_sum[SHA_DIGEST_LENGTH];
	unsigned char *received_check_sum = NULL;

	cout << "Enter file name: \n";
	cin.getline(fname, sizeof(fname));
	cin.getline(fname, sizeof(fname));

	//Check if "Downloads/" dir exists,
	//	if not, it's created.
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

	cout << "Saving to file..." << endl;

	while(true) {
		metadata_file << op << username << fname << total;
		s.send(metadata_file);

		s.receive(answer);

		if(answer.get(0) == "Error") {
			if(atoi(answer.get(1).c_str()) == 0) {
				cout << "\nThe file requested does not exist or couldn't be read!!" << endl;
				return;
			}
		}

		answer >> file_size;
		answer >> size_chunk;

		if (total == 0) {
			f = fopen(path.c_str(), "wb");
		} else {
			f = fopen(path.c_str(), "ab");
			fseek(f, 0L, SEEK_END);
		}

		fflush(f);

		chunks++;
		size = answer.size(2);

		char *data;
		data = (char*) answer.raw_data(2);
		fwrite(data, 1, size, f);
		total += size;
		fclose(f);

		progress = ((double)total/(double)file_size);
		if((progress*100.0) <= 100.0){
			printf("\r[%3.2f%%]",progress*100.0);
		}
		fflush(stdout);

		if (size == 0 || (int)size < size_chunk) {
			req_check_sum << op << username << fname << total;
			s.send(req_check_sum);
			break;
		}
	}

	cout << endl;
	cout << "Chunks received: " << chunks << "\nBytes received: " << total << endl;

	s.receive(answer);
	received_check_sum = (unsigned char *)answer.raw_data(0);

	f = fopen(path.c_str(), "rb");
	assert(f);
	fflush(f);
	fseek(f, 0L, SEEK_SET);

	data_sha1 = (char*) malloc (sizeof(char)*total);
	assert(data_sha1);

	size = fread(data_sha1, 1, total, f);

	SHA1((unsigned char *)data_sha1, size, (unsigned char *)&check_sum);
	
	fclose(f);
	free(data_sha1);

	cout << "Received Check sum: ";
	printChecksum(received_check_sum);

	cout << "Calculated check sum: ";
	printChecksum(check_sum);

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

void uploadFile(socket &s, string op, string username) {
	FILE* f;
	char *data;
	size_t size, offset;
	char fname[100], file_name[100];
	message file_message, server_answer, check_sum_msg;
	double progress;

	unsigned char check_sum[SHA_DIGEST_LENGTH];

	cout << "Enter file name: \n";
	cin.getline(fname, sizeof(fname));
	cin.getline(fname, sizeof(fname));

	getFileName(fname, file_name);

	if(!(f = fopen(fname, "rb"))) {
		cout << "\nThe file requested does not exist or couldn't be read!!" << endl;
		return;
	}
	fflush(f);

	fseek(f, 0L, SEEK_END);
	long sz = ftell(f);
	cout << "\nFile size (bytes): " << sz << endl;
	fseek(f, 0L, SEEK_SET);

	cout << "File to upload: " << file_name << endl;

	offset = 0;
	while(true) {
		fseek(f, offset, SEEK_SET);
		data = (char*) malloc (sizeof(char)*CHUNK_SIZE);
		assert(data);

		size = fread(data, 1, CHUNK_SIZE, f);

		file_message << op << username << file_name << CHUNK_SIZE << offset;

		file_message.push_back(data, size);
		free(data);

		s.send(file_message);

		s.receive(server_answer);

		offset += size;
		progress = ((double)offset/(double)sz);
		if((progress*100.0) <= 100.0){
			printf("\r[%3.2f%%]",progress*100.0);
		}
		fflush(stdout);

		if(server_answer.get(0) == "Error") {
			if(atoi(server_answer.get(1).c_str()) == 0) {
			cout << "\nThe file requested does not exist or couldn't be read!!" << endl;
			return;
			}
		} else if(server_answer.get(0) == "Done") {
			break;
		}
	}

	fseek(f, 0L, SEEK_SET);
	data = (char*) malloc (sizeof(char)*sz);
	assert(data);

	size = fread(data, 1, sz, f);
	/* first argument needs to be an unsigned char pointer
     * second argument is number of bytes in the first argument
     * last argument is our buffer, which needs to be able to hold
     * the 16 byte result of the SHA operation */
    SHA1((unsigned char *)data, size, (unsigned char *)&check_sum);

    cout << "\nCalculated check sum: ";
	printChecksum(check_sum);

	check_sum_msg << op << username << file_name << "Check";

    check_sum_msg.push_back(check_sum, SHA_DIGEST_LENGTH);

    fclose(f);
	free(data);

    s.send(check_sum_msg);

	s.receive(server_answer);

	if(server_answer.get(0) == "Error") {
		if(atoi(server_answer.get(1).c_str()) == 0) {
			cout << "\nThe file requested does not exist or couldn't be read!!" << endl;
			return;
		} else if(atoi(server_answer.get(1).c_str()) == 0) {
			cout << "\nThe Checksum failed, please try again!!" << endl;
		}
	}

	if(server_answer.get(0) == "ok") {
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

int main() {

	message login, response, create_user;
	string op, create, answer;
	char username[40];
	string password = "";
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

			if(op == "Upload" || op == "upload" || op == "up") {
				uploadFile(s, "Upload", username);
			} else if(op == "Download" || op == "download" || op == "down") {
				downloadFile(s, "Download", username);
			} else if(op == "List_files" || op == "list_files" || op == "ls") {
				listFiles(s, "List_files", username);
			} else if(op == "Delete" || op == "delete" || op == "del") {
				deleteFile(s, "Delete", username);
			} else if(op == "Exit" || op == "exit" || op == "ex") {
				break;
			} else {
				cout << "\nInvalid option, please enter one of the listed options!\n";
			}
		}
	}
	else cout << "\nUser not found!!\n\n";

	cout << "Finished\n";
	s.close();
	return 0;
}
