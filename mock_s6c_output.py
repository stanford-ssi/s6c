# This file uses the mc interface to send test data to the mission control suite to verify that the interface works
# Should only be used for testing

import time
import random
from mc_interface import on_downlinked_data, listen_for_data_to_uplink
from min import MINFrame

# Note that in a previous version, we parsed as follows. This may be useful for understanding the format.
#   out = struct.unpack("iii", f.payload[1:13])
#   data = {"latitude": out[0]/1000000, "longitude": out[1]/1000000, "altitude": out[2], "id":str(uuid.uuid4()), "mission": 53, "timestamp": int(time.time()*1000)}


def create_mock_data():
    """
    Starts generating fake data and feeds it to the MC interface

    :return:
    """

    byte_order = 'little'  # Hopefully this is right :skeleton:
    min_id = 0  # message type identifier used by the MIN protocol. We don't use it
    sequence_number = 0  # sequence number used by the MIN protocol. We don't use it
    payload_length = 13
    ascent_rate = 3

    altitude = 0

    while True:
        print('[Mock S6C] Sending datapoint with altitude %d' % altitude)
        # start with random data, and fill in anything useful
        payload = bytearray(random.getrandbits(8) for _ in range(payload_length))
        payload[9:13] = altitude.to_bytes(4, byteorder=byte_order)

        frame = MINFrame(min_id=min_id, payload=bytes(payload), seq=sequence_number, transport=False)
        on_downlinked_data(frame)

        sequence_number += 1
        altitude += ascent_rate

        time.sleep(1)


if __name__ == "__main__":
    listen_for_data_to_uplink(lambda x: print('[Mock S6C] Uplinking', x))
    create_mock_data()