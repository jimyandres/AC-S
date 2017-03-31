#!/usr/bin/python
import zmq
import random
import sys
import time
import hashlib
import json
import string

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

def get_hash(val):
    id = hashlib.sha1()
    id.update(val)
    return str(id.hexdigest())[:6]


def main():
    if len(sys.argv) != 2:
        print 'Enter configuration file'
        exit()

    configFile = open(sys.argv[1], 'r')
    config = json.load(configFile)

    node_name = get_id()
    print("Starting node with name {}".format(node_name))

    context = zmq.Context()
    request = context.socket(zmq.PUSH)
    requestAddress = 'tcp://localhost:{}'.format(config['server']) 
    request.connect(requestAddress)
    
    print('Connect to server on {} and listen for answers on {}'.format(config['server'],config['client']))

    answers = context.socket(zmq.PULL)
    answersAddress = 'tcp://*:{}'.format(config['client'])
    answers.bind(answersAddress)

    poller = zmq.Poller()
    stdin = sys.stdin.fileno()
    poller.register(stdin, zmq.POLLIN)
    poller.register(answers, zmq.POLLIN)

    print('Send something!')
    should_continue = True
    while should_continue:
        print("Iteration")
        socks = dict(poller.poll())
        if stdin in socks and socks[stdin] == zmq.POLLIN:
            print("Input on stdin")
            val = raw_input()
            print("Input is: " + val)
            message = {
                        'id': node_name,
                        'answer_to': 'tcp://localhost:{}'.format(config['client']),
                        'type': 'insert',
                        'key': val, #get_hash(val),
                        'value': val
                    }
            request.send_json(message)
            print("finished")

        if answers in socks and socks[answers] == zmq.POLLIN:
            print("Data on answers!!!")
            req = answers.recv_json()
            if req['status']:
                print 'Server:', req['hash']
            else:
                print 'Error:', req['message']
        should_continue = True


if __name__ == '__main__':
    main()
