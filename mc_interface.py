import os
import threading
import time

uplink_filename = 'uplink.fifo'
downlink_filename = 'downlink.fifo'
downlink_fifo = None

uplink_listener_thread = None
uplink_listeners = []


def create_downlink_fifo():
    global downlink_fifo

    try:
        downlink_fifo = os.open(downlink_filename, os.O_WRONLY | os.O_NONBLOCK)
    except OSError as e:
        if e.errno == 6 or e.errno == 2:
            print('[MC Interface] Failed to open named pipe; MC is not listening')
        else:
            raise e

    if downlink_fifo is not None:
        print("[MC Interface] Outputting data to %s" % downlink_filename)


def on_downlinked_data(frame):
    """
    Called each time a frame of downlinked data comes in

    :param frame: Frame of download data. Should be a MIN frame
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


def uplink_poller():
    """
    Creates a thread to poll the uplink fifo
    Calls all global callbacks

    :return:
    """

    if not os.path.exists(uplink_filename):
        os.mkfifo(uplink_filename)

    uplink_fifo = os.open(uplink_filename, os.O_RDONLY | os.O_NONBLOCK)
    uplink_file = os.fdopen(uplink_fifo)

    print('[MC Interface] Reading from %s' % uplink_filename)

    while True:
        line = uplink_file.readline()

        if not line:
            time.sleep(0.1)
            continue

        for listener in uplink_listeners:
            listener(line)


def listen_for_data_to_uplink(callback):
    """
    Starts listening for uplinked data
    Adds the callback as a listener and starts the polling thread if needed

    :param callback: function to call with the uplinked data
    :return:
    """
    uplink_listeners.append(callback)

    global uplink_listener_thread

    if uplink_listener_thread is None:
        uplink_listener_thread = threading.Thread(target=uplink_poller)
        uplink_listener_thread.daemon = True
        uplink_listener_thread.start()


