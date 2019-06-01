import time 
import os
#import websocket
import json
import uuid
import os
import hmac
import hashlib
import numpy as np
import struct
from min import ThreadsafeTransportMINSerialHandler
import threading
import sys
import re

lock = threading.Lock()

with open('src/RadioInterface.h') as f:
    t = f.read()
    cmds = re.findall('#define MESSAGE_(.*?) (.*)', t)
    cmds = list(map(lambda x: (x[0].lower().replace('_','-'), eval(x[1])), cmds))
cmds = dict(cmds)

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
mode = sys.argv[2] if len(sys.argv) > 2 else "rx"
handler = ThreadsafeTransportMINSerialHandler(port=port)

def whine(ws, error):
    print("[WS] Got error: "+str(error))

def ohp(ws):
    print("[WS] Connection unexpectedly dropped. Retrying...")
    ws.close()

def opened(ws):
    print("[WS] Connection opened")
    lock.release()

def msg(ws, message):
    print("[WS] ", message)

def run_connection():
    '''global ws
    while True:
        t = time.time()*1000.
        sign = t, hmac.new(str.encode(os.environ['KAI_KEY']), str.encode(str(int(t))), hashlib.sha256).hexdigest()
        ws = websocket.WebSocketApp("wss://nasonov-writer.herokuapp.com/%d/%s" % sign,
                                    on_error=whine,
                                    on_message=msg,
                                    on_close=ohp,
                                    on_open=opened)
        ws.run_forever()
'''

def listen():
    outfile = open("data.txt", "a")

    while True:
        frames = handler.poll()
        if frames:
            for f in frames:
                outfile.write(str(f.payload) + '\n')
                outfile.flush()
                print(repr(f.payload))
                out = struct.unpack("iii", f.payload[1:13])
                #print(out)
                data = {"id":str(uuid.uuid4()), "mission": 53, "timestamp": int(time.time()*1000), "latitude": out[0]/1000000, "longitude": out[1]/1000000, "altitude": out[2]}
                print(data)

                try:
                    ws.send(json.dumps(data))
                except Exception as e:
                    print("[WS] error")
                    print(e)

def start_s6c():
    t = threading.Thread(target=listen)
    t.daemon = True
    t.start()

def start_ws():
    t = threading.Thread(target=run_connection)
    t.daemon = True
    t.start()

    # thanks jerry
    lock.acquire()

start_ws()
start_s6c()

cmd_file = 'command'+port.replace('/','.').replace('\\','.')
print('Listening to',cmd_file)

if mode == "tx":
    with open(cmd_file,'w') as f:
        f.write('set-mode 3\nset-continuous 1')

while True:
    if os.path.exists(cmd_file):
        with open(cmd_file) as f:
            t = f.read()
        sp = t.split('\n')
        for cmd in sp:
            cmd = cmd.lstrip().rstrip()
            if len(cmd) == 0: continue
            spec = None
            if cmd[0] < 'a' or cmd[0] > 'z':
                spec = cmd[0]
                cmd = cmd[1:]
            cmd = cmd.split(' ')
            fwd = spec == '>' or spec == '*'
            local = spec is None or spec == '*'
            msg = []
            if cmd[0] not in cmds:
                print('Unknown command',cmd[0])
                continue
            else:
                print('Command',cmd[0], cmds)
            msg.append(cmds[cmd[0]])
            if cmd[0] == 'send':
                s = ' '.join(cmd[1:])
                msg.append(len(s))
                msg.extend(bytes(s, encoding='ascii'))
            else:
                if cmd[0] == 'set-frequency': t = np.float32
                else: t = np.int16
                if len(cmd) > 1:
                    print('hey',msg)
                    msg.extend(list(map(int, t(cmd[1]).tobytes())))
            if fwd:
                cpy = [cmds['send-config'], len(msg)]
                cpy.extend(msg)
                print('queued forwardedframe', cpy)
                handler.send_frame(min_id=0x01, payload=bytes(cpy))
            if local:
                if fwd: time.sleep(5)
                print('queued local frame', msg)
                handler.send_frame(min_id=0x01, payload=bytes(msg))

            
        with open(cmd_file,'w') as f:
            pass
    time.sleep(0.5)
    #print("Sent")
    #inp  = input('')
    #handler.send_frame(min_id=0x01, payload=struct.pack('<bH', 1, 0b01))#bytes("hi", encoding='ascii'))
    
    #break
