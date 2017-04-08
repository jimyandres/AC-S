# 4th Project - Music player

## Synopsis

Now, the project is oriented to streaming sound files (excepting mp3 files, due to licensing issues). According to that, some changes were made:

* The main functions (upload and download) now are working in a different way, the files are sended in parts to the differents servers avalaibles, according to the priority queue that handles the broker.

* The parts of the files to be sendend to the servers are the same size, the chunk size defined in the system.

* Another major improvement added to this version is the hability to stream the sound files downloaded by the client, so it creates a queue (like a play list) of sound files to be played. This feature are supported thanks to the use of the SFML library. This means that the download function its an "Add to playlist" function too.

* The SFML library allows to play, pause and Stop the current playing sound.

**Note:** the broker keeps managing the same functions as the previous version, but oriented to the major function: keep tracking of all parts of a file uploaded on any server.

## Installation

This version now needs a new library: [SFML](https://www.sfml-dev.org/index.php) (Simple and Fast Multimedia Library) which offers the features to play sounds files, as mentioned before.

The installation its as simple as typing on terminal (on linux):
```
sudo apt-get install libsfml-dev
```

## Running the program

To run the program compile with ```. export.sh```, then:
* Run broker (the broker will always run on port 5556 for servers and port 5555 for clients. The address will be the default ipv4 address of the computer that is running on: ```./broker```
* Run servers: ```./fileServer server_address:port broker_address:5556```
* Run clients: ```./fileClient broker_address:5555```

The server address cannot be localhost, instead use 127.0.0.1.