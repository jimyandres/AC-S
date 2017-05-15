# Map Reduce

## Synopsis

This project is an implementation of MapReduce programming paradigm
where uses its principle to achieve a purpose: count the use
of words in an input file, like a book.

There are three main components:
* **Master:** As the name implies, is the main program who controls the
  communications between him and the other components (Mappers and
  Reducers). It will be the responsible of dividing the input text by the
  total Mappers registered, send each part and finally receive from the
  Reducers the result of the task.
* **Mapper:** This one, will receive his part to be tokenized by words
  and count them. Then stores the frequency of each word in a
  unordered map which will be the input for the Reducers.
* **Reducer:** Finally, each Reducer will be responsible for a part of
  the total range supported by the program, for example a part of the
  alphabet. This program will receive from each Mapper the corresponding
  words, finally reduces them and sends the results again to
  the Master program.


## Installation

For this project to work you have to install the next items:
* [zmq](http://zeromq.org)
* zmqpp

To install zmq and zmqpp follow the next steps:
Download zmq from [here](http://zeromq.org/intro:get-the-software),
Go to Downloads directory
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

Once these dependencies are installed you can compile the code by
either executing the export.sh bash file, like this: ```. export.sh```.
Or running the Makefile, in terminal just go to the folder in which
the files are saved and type ```make```.

## Running the program

To run the program compile with ```. export.sh```, then:
* Run Master: ```./master <file_path> <bootstrap_address> ```
* Run Mapper: ```./mapper <bootstrap_address> <master_address:5555>```
* Run Reducer: ```./reducer <bootstrap_address> <my_address>
  <lower_limit> <upper_limit>```

**Notes:**
* The ```bootstrap_address``` will be the same for each program.
* ```<lower_limit>``` and ```<upper_limit>``` will define the alphabetic
  range of the reducer, E.g. a z for a range of a-z.

The server address cannot be localhost, instead use 127.0.0.1.
