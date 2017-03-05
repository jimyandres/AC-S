# 3rd Project - File Server V3

## Synopsis

On this version of the file server project several changes were made. The most significant one is that that a broker was introduced, this broker now handles the functions of creating users, validating users, and list files. It also receives all upload requests from clients and according to the priority queue that it has for load balancing, responds with the address of the server to which the client has to connect to complete the action. Similar process is followed to download request, the broker receives the request from a client, looks on which server is the requested file stored and responds with the server address to connect to.

The broker keeps record of all the files (the checksum of the file and the server address in which the file is stored) uploaded on a json file.
The users information is stored in another json file. The list files function operates with the list of files that were uploaded by an user, which is stored in said users json file.

## Installation

This version of the project no longer depends on this: json: ```sudo apt-get install libjsoncpp-dev libjsoncpp1```.
To run the program compile with ```. export.sh```, then:
* Run broker (the broker will always run on port 5556 for servers and port 5555 for clients. The address will be the default ipv4 address of the computer that is running on: ./broker
* Run servers: ./fileServer address:port broker_address:5556 delay
* Run clients: ./fileClient broker_address:5555

If no delay is wanted, put a 0. The delay value is in seconds.