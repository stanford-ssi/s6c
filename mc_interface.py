import os

downlink_filename = 'downlink.fifo'
downlink_fifo = None


def create_downlink_fifo():
    global downlink_fifo

    try:
        downlink_fifo = os.open(downlink_filename, os.O_WRONLY | os.O_NONBLOCK)
    except OSError as e:
        if e.errno == 6:
            print('[MC Interface] Failed to open named pipe; MC is not listening')
        else:
            raise e

    print("[MC Interface] Outputting data to %s" % downlink_filename)


def on_downlinked_data(frame):
    """
    Called each time a frame of downlinked data comes in

    :param frame:
    :return:
    """
    global downlink_fifo

    print("[MC Interface] Received downlinked data", frame)

    if downlink_fifo is None:
        create_downlink_fifo()

    if downlink_fifo is not None:
        try:
            os.write(downlink_fifo, frame.payload)
        except OSError as e:
            if e.errno == 32:
                print('[MC Interface] Downlink pipe broken; closing')
                os.close(downlink_fifo)
                downlink_fifo = None
            else:
                raise e
