#!/usr/bin/python
import zmq
import random
import sys
import time
import hashlib
import json
import string

# State:
hash_table = {}  # to store the keys for this node
clients = {}     # to store the identities and response addresses of clients
node_name = ''   # node dentifier
context = ''
lower_bound = '' # predecessor's id
upper_bound = '' # successor's id

# Returns a randomly generated identifier for the node.
# TODO: This is weak! 
def get_id(simple=True, length = 6):
    if (simple):
        possible = list(string.ascii_lowercase)
        rnd = random.randint(0,len(possible))
        return str(possible[rnd])
    else:
        rnd = random.randint(0,100)
        id = hashlib.sha1()
        id.update(str(rnd))
        return str(id.hexdigest())[:length]

# Tests whether this node is responsible for storing key 
def in_range(key):
    if upper_bound >= lower_bound:
        return key > lower_bound and key <= upper_bound
    else:
        return key >= lower_bound or key < upper_bound

def interval():
    return '({},{}]'.format(lower_bound,upper_bound)

def localOP(request):
    operation = request['type']
    if operation == 'insert':
        key = request['key']
        value = request['value']
        hash_table[key] = value
        print("Stored {} {} at {}".format(key,value,node_name))
    else:
        print("Local operation not implemented")


def handleClientRequest(request, successorSocket):
    clientId = request['id']
    clientAddress =  request['answer_to']
    sc = context.socket(zmq.PUSH)
    sc.connect(clientAddress)
    clients[clientId] = sc

    key = request['key']
    if in_range(key):
        print("Key {} is mine!!!!".format(key))
        localOP(request)
    else:
        print("Key {} is not mine, delegating...".format(key))
        successorSocket.send_json(request)

def main():
    global hash_table, clients, node_name, context, lower_bound, upper_bound

    if len(sys.argv) != 2:
        print 'Enter configuration file'
        exit()

    configFile = open(sys.argv[1], 'r')
    config = json.load(configFile)

    node_name = get_id()
    upper_bound = node_name
    print("Node name: {}".format(node_name))

    myPort = config['port']
    succPort = config['successor']
    clientPort = config['client']
    bootstrap = config['bootstrap']

    print("Listening on {} and connectig to neighbor on {}".format(myPort, succPort))

    context = zmq.Context()
    mySocket = context.socket(zmq.PAIR)
    mySocket.bind("tcp://*:" + myPort)

    successorSocket = context.socket(zmq.PAIR)
    successorSocket.connect("tcp://localhost:" + succPort)

    if bootstrap:
        # The first step is to send the node's identifier to the succcesor.
        # That way every node computes the range of keys it is responsible 
        # for.
        message = {'type': 'send-id', 'data': node_name}
        successorSocket.send(json.dumps(message))

        # the second step is to wait for the predecessor's identifier
        req = json.loads(mySocket.recv())
        lower_bound = req['data']

        #print('({}, {}]'.format(req['data'], node_name))
        print("Responsible for keys in {}".format(interval()))
    else:
        print("Not implemented yet!")
        exit()

    # All the client's requests from clients arrive to this socket
    client_socket = context.socket(zmq.PULL)
    client_socket.bind("tcp://*:" + clientPort)
    print 'Listening to clients on {}'.format(clientPort)

    poller = zmq.Poller()
    poller.register(mySocket, zmq.POLLIN)
    poller.register(client_socket, zmq.POLLIN)

    should_continue = True
    while should_continue:
        print("Iteration")
        socks = dict(poller.poll())
        if mySocket in socks and socks[mySocket] == zmq.POLLIN:
            req = mySocket.recv_json()
            handleClientRequest(req, successorSocket)

        if client_socket in socks and socks[client_socket] == zmq.POLLIN:
            print("Message on client's socket")
            req = client_socket.recv_json()
            handleClientRequest(req, successorSocket)

        should_continue = True

if __name__ == '__main__':
    main()
