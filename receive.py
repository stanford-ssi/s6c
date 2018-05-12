import websocket
import json
import uuid
import os
import hmac
import hashlib
import time
import threading
import re
import string

last_message = time.time()

def heartbeat():
    while True:
        if time.time() - last_message  > 16:
            print("heartbeat violation!")
            os._exit(1)
        time.sleep(1)

t = threading.Thread(target=heartbeat)
t.daemon = True
t.start()

def clean(s):
    return ''.join([x for x in s.lower() if x in (string.ascii_lowercase + "_")])

fmt = open('fmt.txt').read()

a = re.findall(r'radio\.addVariable\(data\.(.*?)\s*,\s*(\-*\d+)\s*,\s*(\-*\d+)\s*,\s*(\-*\d+)\s*\);(.*?)$', fmt, re.MULTILINE)

vars = []
for var in a:
    n = clean(var[0])
    if var[4] != '':
       n = var[4].replace('//','').rstrip().lstrip()
    n = n.replace(' ','_')
    vars.append([n, int(var[1]), int(var[2]), int(var[3])])
    print(vars[-1])

lock = threading.Lock()
lock.acquire()

count = 0
recv = 0

def whine(ws, error):
    print("[WS] Got error: "+str(error))

def ohp(ws):
    print("[WS] Connection unexpectedly dropped. Retrying...")
    ws.close()

def opened(ws):
    print("[WS] Connection opened")
    lock.release()

def msg(ws, message):
    global count
    global recv
    if 'success' in message:
        count += 1
    if 'receive' in message:
        recv += 1
    #print("[WS] Got message", message)
    global last_message
    last_message = time.time()

def run_connection():
    global ws
    while True:
        t = time.time()*1000.
        sign = t, hmac.new(str.encode(os.environ['KAI_KEY']), str.encode(str(int(t))), hashlib.sha256).hexdigest()
        ws = websocket.WebSocketApp("wss://nasonov-writer.herokuapp.com/%d/%s" % sign,
                                    on_error=whine,
                                    on_message=msg,
                                    on_close=ohp,
                                    on_open=opened)
        ws.run_forever()


t = threading.Thread(target=run_connection)
t.daemon = True
t.start()

lock.acquire()
print("Okay, we're in business")

import serial
s = serial.Serial('/dev/ttyACM0', 57600)

rolling = []

START = [204, 105, 119, 82]
END = [162, 98, 128, 161]

parsing = False
message = []

import struct

def parse_message(msg):
    #print("Parsing", msg, len(msg))
    try:
        #msg = list(map(chr, msg))
        rssi = int(msg[0])
        recc = struct.unpack('I', bytearray(msg[1:5]))[0]
        drop = struct.unpack('I', bytearray(msg[5:9]))[0]
        #print("RSSI", rssi)
        print("Received", recc)
        #print("Dropped", drop)
        #ln = ord(sp[1])
        aa = msg[9:]
        inp = ""
        for c in aa:
           num = c
           for i in [1,2,4,8,16,32,64,128][::-1]:
              inp += ("1" if (num & i) else "0")
        dd = {"id":str(uuid.uuid4()), "mission": 48, "timestamp": int(time.time()*1000), "msgs_received": recc, "msgs_lost": drop, "rssi": rssi}#, "received": msg[:ln].encode('hex')}
        for name, min, max, bits in vars:
            x = inp[0:bits]
            inp = inp[bits:]
            x = int(x, 2)
            v = min + (max-min) * x / (2**bits - 1.)
            dd[name] = v
            #print(name, v)
        #print(list(dd.keys()))
        #print "actual thing received", msg[:ln]
        #print "raw bytestring", msg[ln:]

        ws.send(json.dumps(dd))

    except ImportError:
        print("Error parsing")

def check_comm():
    while True:
        with open("comm.txt") as c:
            t = c.read().lstrip().rstrip()
        if t != "":
            print("SENDING MESSAGE"+"\n"*10)
            s.write(str.encode(">"+t+"<"))
            print("sent it a message")
            with open("comm.txt","w") as c:
                c.write("")
        time.sleep(0.2)

t = threading.Thread(target=check_comm)
t.daemon = True
t.start()

import sys
while True:
    b = ord(s.read(1))
    rolling.append(b)
    if len(rolling) > 4:
        rolling = rolling[1:]
    if rolling == START:
        parsing = True
        message = []
    elif rolling == END:
        parsing = False
        parse_message(message[:-3])
    elif parsing:
        message.append(b)
    else:
        sys.stdout.write(chr(b))
