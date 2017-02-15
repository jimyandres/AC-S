#!/bin/bash

make

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$HOME/zmq/lib
#env | grep '^LD_LIBRARY_PATH='
