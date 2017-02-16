#include <iostream>
#include <string>
#include <zmqpp/zmqpp.hpp>
#include <sys/stat.h>

#include <openssl/md5.h>

#include <unistd.h> 		//Get the user id of the current user
#include <sys/types.h>
#include <pwd.h>			//Get the password entry (which includes the home directory) of the user

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
	string file_name, path;
	message metadata_file, answer, data_request;
	size_t size;
	char *data;
	FILE* f;

	unsigned char check_sum[MD5_DIGEST_LENGTH];
	unsigned char *received_check_sum;

	cout << "Enter file name: \n";
	cin.getline(fname, sizeof(fname));
	cin.getline(fname, sizeof(fname));

	metadata_file << op << username << fname;
	s.send(metadata_file);

	s.receive(answer);

	if(answer.get(0) == "Error") {
		if(atoi(answer.get(1).c_str()) == 0) {
			cout << "\nThe file requested does not exist or couldn't be read!!" << endl;
			return;
		}
	}

	answer >> size;
	answer >> file_name;

	cout << "File size: " << size << endl;

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


	path.append(file_name);

	cout << "Path: " << path << endl;

	f = fopen(path.c_str(), "wb");
	assert(f);
	fflush(f);
	fseek(f, 0L, SEEK_SET);

	data = (char*)answer.raw_data(2);
	received_check_sum = (unsigned char *)answer.raw_data(3);

	MD5((unsigned char *)data, size, (unsigned char *)&check_sum);
	
	cout << "Calculated check sum: ";
	printChecksum(check_sum);

	if(memcmp(received_check_sum, check_sum, MD5_DIGEST_LENGTH)) {
		cout << "\nThe Checksum failed, please try again!!" << endl;
		return;
	}

	cout << "Saving to file..." << endl;

	fwrite(data, 1, size, f);
	fclose(f);

	cout << "File saved successfully\n";
}

void uploadFile(socket &s, string op, string username) {
	FILE* f;
	char *data;
	size_t size;
	char fname[100], file_name[100];
	message metadata_message, server_answer, data_message;

	unsigned char check_sum[MD5_DIGEST_LENGTH];

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

	data = (char*) malloc (sizeof(char)*sz);
	assert(data);

	size = fread(data, 1, sz, f);

	metadata_message << op << username << file_name << size;

	metadata_message.push_back(data, size);

	/* first argument needs to be an unsigned char pointer
     * second argument is number of bytes in the first argument
     * last argument is our buffer, which needs to be able to hold
     * the 16 byte result of the MD5 operation */
    MD5((unsigned char *)data, size, (unsigned char *)&check_sum);

    cout << "Calculated check sum: ";
	printChecksum(check_sum);

    metadata_message.push_back(check_sum, MD5_DIGEST_LENGTH);


	if(s.send(metadata_message)) {
		cout << "Message containing \""<< file_name << "\" has been sended successfully\n";
	} else {
		cout << "Message containing \""<< file_name << "\" hasnt been sended successfully\n";
	}

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

	fclose(f);
	free(data);
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
