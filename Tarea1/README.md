# 1st Project - File Server

## Synopsis

This project is about a File Server implemented on C++. The server allows to the client: List Files that the user owns, Download files previously uploaded, Upload new files and Delete his own files.
If you’re testing the project running both server and client programs on the same computer, you have to execute each in different terminal windows.

If it's the first time executing the program, you will have to create an user. The next time executing, say no when the program asks if you wish to create an user

## Installation

For this project to work you have to install the next items:
* [zmqp](http://zeromq.org)
* zmqpp
* json: ```sudo apt-get install libjsoncpp-dev libjsoncpp1```
* openssl: ```sudo apt-get install openssl``` and ```sudo apt-get install libssl-dev```

To install zmq and zmqpp follow the next steps:
Download zmq from [here](http://zeromq.org/intro:get-the-software), Go to Downloads directory
```
tar xvfz zeromq-4.2.1.tar.gz
mkdir buildzmq && cd buildzmq
cmake -DCMAKE_INSTALL_PREFIX=$HOME/zmq ../zeromq-4.2.1
make -j4
make install
cd ..
git clone https://github.com/zeromq/zmqpp.git
mkdir buildzmqpp && cd buildzmqpp
cmake -DCMAKE_INSTALL_PREFIX=$HOME/zmq ../zmqpp
make -j4
make install
```

Once these dependencies are installed you can compile the code by either executing the export.sh bash file, like this: ```. export.sh```. Or running the Makefile, in terminal just go to the folder in which the files are saved and type ```make```.

## Tests

This project is made for Linux distributions.
First execute the export.sh file, It will compile the fileClient.cc and fileServer.cc files. Then run ```./fileServer <<server_address>>``` and ```./fileClient```, the "server_addres" argument is the address in which the server will be running, for example: localhost:5555. The client program will guide you to the options that the server provide, like create a new user or just login. After login, it will show you the major options like List_files, Download, Upload, Delete and Exit.

According to the selected option, the program will ask you some information like location of the file to upload or the name of the file to download, etc.