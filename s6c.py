import time
import numpy as np
from min import ThreadsafeTransportMINSerialHandler
import sys
import re
from mc_interface import on_downlinked_data, listen_for_data_to_uplink

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

        time.sleep(0.01)


def send_command_to_s6c(command):
    # validate command
    command = command.lstrip().rstrip()

    if len(command) == 0:
        return

    spec = None
    if command[0] < 'a' or command[0] > 'z':
        spec = command[0]
        command = command[1:]

    command = command.split(' ')
    fwd = spec == '>' or spec == '*'
    local = spec is None or spec == '*'
    msg = []

    if command[0] not in cmds:
        print('Unknown command', command[0])
        return
    else:
        print('Command', command[0], cmds)

    # Parse it
    msg.append(cmds[command[0]])

    if command[0] == 'send':
        command_string = ' '.join(command[1:])
        msg.append(len(command_string))
        msg.extend(bytes(command_string, encoding='ascii'))
    else:
        if command[0] == 'set-frequency':
            data_type = np.float32
        else:
            data_type = np.int16

        if len(command) > 1:
            print('hey', msg)
            msg.extend(list(map(int, data_type(command[1]).tobytes())))

    # Send it
    if fwd:
        cpy = [cmds['send-config'], len(msg)]
        cpy.extend(msg)
        print('queued forwardedframe', cpy)
        handler.send_frame(min_id=0x01, payload=bytes(cpy))

    if local:
        if fwd: time.sleep(5)
        print('queued local frame', msg)
        handler.send_frame(min_id=0x01, payload=bytes(msg))

if mode == "tx":
    send_command_to_s6c('set-mode 3')
    send_command_to_s6c('set-continuous 1')

listen_for_data_to_uplink(send_command_to_s6c)

listen_to_s6c()
