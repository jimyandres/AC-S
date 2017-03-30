#!/usr/bin/python
import zmq
import random
import sys
import time
import hashlib
import json

hash_table = {}
lower_bound = ''
upper_bound = ''


def get_id():
    id = hashlib.sha1()
    id.update(str(random.randint(0,100)))
    return str(id.hexdigest())


def in_range(key):
    if upper_bound >= lower_bound:
        return key > lower_bound and key <= upper_bound
    else:
        return key >= lower_bound or key < upper_bound


def main():
    global hash_table, lower_bound, upper_bound

    if len(sys.argv) != 4:
        print 'Enter 3 ports'
        exit()

    node_name = get_id()
    upper_bound = node_name
    print("Node name: {}".format(node_name))

    myPort = sys.argv[1]
    neighborPort = sys.argv[2]
    client_port = sys.argv[3]
    print("Listening on {} and connectig to neighbor on {}".format(myPort, neighborPort))

    context = zmq.Context()
    mySocket = context.socket(zmq.PAIR)
    mySocket.bind("tcp://*:" + myPort)

    mySuccessor = context.socket(zmq.PAIR)
    mySuccessor.connect("tcp://localhost:" + neighborPort)

    # Send my id
    message = {}
    message['type'] = 'send-id'
    message['data'] = node_name
    mySuccessor.send(json.dumps(message))

    # Receive successor's id
    req = json.loads(mySocket.recv())
    lower_bound = req['data']
    print '({}, {}]'.format(req['data'], node_name)

    client_socket = context.socket(zmq.REP)
    client_socket.bind("tcp://*:" + client_port)
    print 'Listening to clients on {}'.format(client_port)
    while True:
        req = json.loads(client_socket.recv())
        print req['key'], req['value'], 'in_range:', in_range(req['key'])

        if in_range(req['key']):
            res = {
                'status': True,
                'hash': req['key']
            }
        else:
            res = {
                'status': False,
                'message': 'Not my responsability'
            }
        client_socket.send(json.dumps(res))


if __name__ == '__main__':
    main()
