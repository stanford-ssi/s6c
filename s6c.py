import time
import os
import numpy as np
from min import ThreadsafeTransportMINSerialHandler
import threading
import sys
import re
from mc_interface import on_downlinked_data

lock = threading.Lock()

with open('src/RadioInterface.h') as f:
    t = f.read()
    cmds = re.findall('#define MESSAGE_(.*?) (.*)', t)
    cmds = list(map(lambda x: (x[0].lower().replace('_','-'), eval(x[1])), cmds))
cmds = dict(cmds)

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
mode = sys.argv[2] if len(sys.argv) > 2 else "rx"
handler = ThreadsafeTransportMINSerialHandler(port=port)

# Listens to output from the s6c, logging it all to data.txt
def listen_to_s6c():
    outfile = open("data.txt", "a")

    while True:
        frames = handler.poll()
        if frames:
            for frame in frames:
                outfile.write(str(frame.payload) + '\n')
                outfile.flush()

                on_downlinked_data(frame)

# Starts a thread to listen to the s6c
def start_s6c():
    s6c_listener_thread = threading.Thread(target=listen_to_s6c)
    s6c_listener_thread.daemon = True
    s6c_listener_thread.start()

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
