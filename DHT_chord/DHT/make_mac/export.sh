#!/bin/bash

make

export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$HOME/zmq/lib
cd ..
