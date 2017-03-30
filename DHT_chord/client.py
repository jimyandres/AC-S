#!/usr/bin/python
import zmq
import random
import sys
import time
import hashlib
import json


def get_hash(val):
    id = hashlib.sha1()
    id.update(val)
    return str(id.hexdigest())


def main():
    if len(sys.argv) != 2:
        print 'Enter server port'
        exit()

    context = zmq.Context()
    socket = context.socket(zmq.REQ)
    socket.connect('tcp://localhost:{}'.format(sys.argv[1]))

    print 'Send something!'
    while True:
        val = raw_input()
        message = {
            'type': 'post',
            'key': get_hash(val),
            'value': val
        }
        socket.send(json.dumps(message))

        req = json.loads(socket.recv())
        if req['status']:
            print 'Server:', req['hash']
        else:
            print 'Error:', req['message']


if __name__ == '__main__':
    main()
