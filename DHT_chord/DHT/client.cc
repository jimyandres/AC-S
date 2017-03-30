#include <iostream>
//#include <stdio.h>
#include <string.h>
#include <zmqpp/zmqpp.hpp>
#include <openssl/sha.h>

using namespace std;
using namespace zmqpp;

void ChecksumToString(unsigned char * check_sum, char mdString[SHA_DIGEST_LENGTH*2+1]) {
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)check_sum[i]);
}

void getID(string Value, char ans[SHA_DIGEST_LENGTH*2+1]) {
	unsigned char check_sum[SHA_DIGEST_LENGTH];
	
	SHA1((unsigned char *)Value.c_str(), Value.size(), (unsigned char *)&check_sum);
	ChecksumToString((unsigned char *)&check_sum, ans);
}

int main(int argc, char** argv) {
	if(argc != 2) {
		cout << "Enter server address!" << endl;
		return EXIT_FAILURE;
	}

	context ctx;
	socket serverSocket(ctx, socket_type::req);	
	serverSocket.connect("tcp://localhost:" + string(argv[1]));

	cout << "Enter something to send: " << endl;
	while(true) {
		string val, status, hash, err_msg;
		char key[SHA_DIGEST_LENGTH*2+1];
		getline(cin, val);
		getID(val, key);

		message req, res;
		req << "Post" << key << val;
		serverSocket.send(req);
		serverSocket.receive(res);

		res >> status;
		if(status == "Ok") {
			res >> hash;
			cout << "Hash: " << hash << endl;
		} else if(status == "Error") {
			res >> err_msg;
			cout << "Error: " << err_msg << endl;
		}
		//cout << key << req << endl;
	}

	serverSocket.close();
	ctx.terminate();
	return 0;
}