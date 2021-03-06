#include <iostream>
#include <stdio.h>
#include <string.h>
#include <zmqpp/zmqpp.hpp>
#include <sys/stat.h>

#include <openssl/sha.h>
#include "src/json.hpp"
#include <queue>
#include <SFML/Audio.hpp>
#include <unistd.h> 		//Get the user id of the current user
#include <sys/types.h>
#include <pwd.h>			//Get the password entry (which includes the home directory) of the user

#define CHUNK_SIZE 5242880

using namespace std;
using namespace zmqpp;
using json = nlohmann::json;

queue<sf::SoundBuffer*> play_list;

void ChecksumToString(unsigned char * check_sum, char mdString[SHA_DIGEST_LENGTH*2+1]) {
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)check_sum[i]);
}

void printMenu() {
	cout << "\n\n***********************************\n";
	cout << "Enter action to perform: \n\t(ls) List uploaded files\n\t(add) Add to play list\n\t(up) Upload song\n\t(del) Delete song\n\t(p) Reproduce/Pause play list\n\t(s) Stop play list\n\t(menu) Show menu\n\t(ex) Exit\n";
	cout << "\n";
}

void disconnectFromServer(socket &s, string address) {
	//cout << "Disconnected from " << address << endl;
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

void SaveFile(message &answer) {
	string path, fname, server_fname, file_size_str, nPart;
	size_t size;
	int size_chunk;
	char *data;
	FILE* f;

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
	answer >> nPart;
	answer >> file_size_str;

	if (nPart == "0") {
		f = fopen(path.c_str(), "wb");
	} 
	else {
		f = fopen(path.c_str(), "ab");
		fseek(f, 0L, SEEK_END);
	}

	answer >> size_chunk;
	size = answer.size(7);
	data = (char*) answer.raw_data(7);

	fwrite(data, 1, size, f);
	fclose(f);
}

void downloadFile(socket &broker_socket, socket &download_socket, string op, string username) {
	string server_fname, fname, file_size_str, servers_address;
	message request_broker, response_broker, metadata_file, server_response;
	size_t offset = 0;
	string offset_str = to_string((int)offset);
	json locations;

	cout << "Enter file name: (q to cancel)" << endl;
	getline(cin, fname);

	if(fname == "cancel" || fname == "q") {
		cout << "Action canceled!!" << endl;
		return;
	}

	request_broker << "Download" << username << fname;
	broker_socket.send(request_broker);
	broker_socket.receive(response_broker);

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
	int num = 0;
	for (int i = 0; i < count; ++i) {
		//cout << "Connected to: " << locations[i] << endl;
		download_socket.connect(locations[i]); //connect to server
		//cout << "Saving part " << i << "..." << endl;
		string part = to_string((int)num);
		metadata_file << "" << op << fname << server_fname << part << "0" ;
		download_socket.send(metadata_file);

		download_socket.receive(server_response);
		server_response >> op; // empty
		server_response >> op;
		if (op == "Download")
			SaveFile(server_response);
		else if (op == "Error") {
			string type;
			server_response >> type;
			cout << "Error downloading file: " << type << endl;
			break;
		} else {
			cout << "Error downloading file " << op << endl;
			break;
		}
		num++;

		disconnectFromServer(download_socket, locations[i]);		
	}
	struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;
	string path = (string)homedir + "/Descargas/Downloads/" + fname;

	sf::SoundBuffer* song = new sf::SoundBuffer();
	play_list.push(song);
	play_list.back()->loadFromFile(path);
	cout << fname << " added to play list" << endl;
}

void uploadFile(socket &broker_socket, socket &upload_socket, string op, string username) {
	FILE* f;
	char *data;
	size_t size;
	char file_name[100], file_SHA1[SHA_DIGEST_LENGTH*2+1];
	message file_message, request_broker, response_broker;
	string servers_address, fname, file_size_str;
	double progress;
	json locations;

	cout << "Enter file name: (q to cancel)" << endl;
	getline(cin, fname);

	if(fname == "cancel" || fname == "q") {
		cout << "Action canceled!!" << endl;
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
		cout << "Error: " <<  servers_address << endl;
		return;
	} else {
		response_broker >> servers_address;
		locations = json::parse(servers_address);
	}

	for (int i = 0; i < (int)locations.size(); ++i)
	{
		//cout << "Connected to: " << locations[i] << endl;
		upload_socket.connect(locations[i]); //connect to server

		file_size_str = to_string(CHUNK_SIZE);
		//cout << "File to upload: " << file_name;
		//cout << "\tPart to upload: " << i << "\n";
		data = (char*) malloc (sizeof(char)*CHUNK_SIZE);
		size = fread(data, 1, CHUNK_SIZE, f);
		string file_SHA1_str(file_SHA1);
		file_message << "" << op << username << fname << file_SHA1_str+"."+to_string(i) << to_string(size) << /*offset_str <<*/ CHUNK_SIZE;
		file_message.push_back(data, size);
		free(data);

		progress = (double)(i+1)/locations.size();
		if((progress*100.0) <= 100.0){
			printf("\r[%3.2f%%]",progress*100.0);
		}
		fflush(stdout);

		upload_socket.send(file_message);
		disconnectFromServer(upload_socket, locations[i]);
	}
	fclose(f);
	cout << endl;
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
	string status, filename, servers_address, server_fname, file_size, ans;
	bool state = false;

	cout << "Enter file name: (q to cancel)" << endl;
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
	response >> server_fname >> file_size >> servers_address;
	json locations = json::parse(servers_address);

	int total_size = atol(file_size.c_str());

	int count = locations.size();
	for (int i = 0; i < count; ++i)
	{
		//cout << "Connected to: " << locations[i] << endl;
		server.connect(locations[i]);
		server_req << "" << "Delete" << server_fname+"."+to_string(i);
		if (total_size < CHUNK_SIZE)
			server_req << to_string(total_size);
		else
			server_req << to_string(CHUNK_SIZE);
		server.send(server_req);
		total_size -= CHUNK_SIZE;
		server.receive(response);
		response >> status; // empty
		response >> ans;
		response >> status;
		if(ans == "DeleteDone") {
			if(status == "ok") {
				state = true;
			} else if(status == "Error") {
				state = false;
				response >> ans;
				cout << ans << endl;
			}
		}

		disconnectFromServer(server, locations[i]);
	}
	if(state) {
		cout << "File: " << filename << " deleted!!" << endl;
	}
}

void deletePlayList() {
	while(!play_list.empty()) {
		delete play_list.front();
		play_list.pop();
	}
}

int main(int argc, char* argv[]) {
	bool playing_flag = false;
	message login, response, create_user, server_response;
	string op, create, answer;
	char username[40];
	string password = "";
	int access;
	string broker_address = "tcp://";
	sf::Sound player;

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
				if(p.poll(10)) {
					if(p.has_input(s)){
						s.receive(server_response);
						//messageHandler(server_response, s, username);
						cout << "Message from some server" << endl;
					}
					if(p.has_input(broker_socket)) {
						cout <<"Message" << endl;
					}
					if(p.has_input(standardin)) {
						getline(cin, op);
						if(op == "Upload" || op == "upload" || op == "up") {
							uploadFile(broker_socket, s, "Upload", username);
						} else if(op == "Download" || op == "download" || op == "down" || op == "add") {
							downloadFile(broker_socket, s, "Download", username);
						} else if(op == "List_files" || op == "list_files" || op == "ls") {
							listFiles(broker_socket, "List_files", username);
						} else if(op == "Delete" || op == "delete" || op == "del") {
							deleteFile(broker_socket, "Delete", username, s);
						} else if(op == "menu") {
							printMenu();
						} else if(op == "p") {
							if(player.getStatus() == sf::Sound::Stopped) {
								if(!playing_flag) {
									if(!play_list.empty()) {
										player.setBuffer(*(play_list.front()));
										player.play();
										playing_flag = true;
									} else {
										cout << "There is nothing on the playlist!!" << endl;
									}
								} else {
									delete play_list.front();
									play_list.pop();
									if(!play_list.empty()) {
										player.setBuffer(*(play_list.front()));
										player.play();
										playing_flag = true;
									} else {
										cout << "There is nothing more on the playlist!!" << endl;
										playing_flag = false;
									}
								}
							} else if(player.getStatus() == sf::Sound::Paused) {
								player.play();
							} else {
								player.pause();
							}
						} else if(op == "s") {
							player.stop();
							if(!play_list.empty()) {
								delete play_list.front();
								play_list.pop();
							}
							playing_flag = false;
						} else if(op == "Exit" || op == "exit" || op == "ex") {
							break;
						} else {
							cout << "\nInvalid option, please enter one of the listed options!" << endl;
						}
					}
				} else if(player.getStatus() == sf::Sound::Stopped) {
					if(playing_flag) {
						delete play_list.front();
						play_list.pop();
						if(!play_list.empty()) {
							player.setBuffer(*(play_list.front()));
							player.play();
							playing_flag = true;
						} else {
							cout << "There is nothing more on the playlist!!" << endl;
							playing_flag = false;
						}
					}
				}
			}
		} else
			cout << "\nUser not found!!\n\n";
	}
	cout << "\nFinished\n";
	deletePlayList();
	broker_socket.close();
	s.close();
	ctx.terminate();
	return 0;
}
