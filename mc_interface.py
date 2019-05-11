import os
import threading
import time

is_windows = os.name == 'nt'

if is_windows:
    import win32pipe, win32file, pywintypes

uplink_filename = 'uplink.fifo'
downlink_filename = 'downlink.fifo'
downlink_fifo = None

uplink_listener_thread = None
uplink_listeners = []


def create_downlink_fifo_windows():
    global downlink_fifo

    try:
        downlink_fifo = win32pipe.CreateNamedPipe(
            '\\\\.\\' + downlink_filename,
            win32pipe.PIPE_ACCESS_DUPLEX,
            win32pipe.PIPE_TYPE_MESSAGE | win32pipe.PIPE_READMODE_MESSAGE | win32pipe.PIPE_WAIT,
            1, 65536, 65536,
            0,
            None)
        print('[MC Interface] Pipe opened')
    except pywintypes.error as e:
        if e.args[0] == 2:
            print('[MC Interface] Failed to open named pipe; MC is not listening')
        else:
            raise e


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
        if is_windows:
            create_downlink_fifo_windows()
        else:
            create_downlink_fifo()

    if downlink_fifo is None:
        return

    try:
        if is_windows:
            win32pipe.ConnectNamedPipe(downlink_fifo, None)
            win32file.WriteFile(downlink_fifo, frame.payload)
        else:
            os.write(downlink_fifo, frame.payload)
    except OSError as e:
        if e.errno == 32:
            print('[MC Interface] Downlink pipe broken; closing')
            os.close(downlink_fifo)
            downlink_fifo = None
        else:
            raise e


def create_uplink_fifo_windows():
    while True:
        try:
            print('[MC Interface] Trying to create uplink pipe')
            uplink_fifo = win32file.CreateFile(
                '\\\\.\\' + uplink_filename,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0,
                None,
                win32file.OPEN_EXISTING,
                0,
                None
            )
            response_code = win32pipe.SetNamedPipeHandleState(uplink_fifo, win32pipe.PIPE_READMODE_MESSAGE, None, None)
            print('[MC Interface] Pipe opened with code %s' % response_code)
            return uplink_fifo
        except pywintypes.error as e:
            if e.args[0] == 2:
                print('[MC Interface] Uplink pipe not created; trying again')
                time.sleep(1)
            elif e.args[0] == 109:
                print('[MC Interface] Uplink pipe broken')
                return None
            else:
                raise e


def uplink_poller():
    """
    Creates a thread to poll the uplink fifo
    Calls all global callbacks

    :return:
    """

    if is_windows:
        uplink_fifo = create_uplink_fifo_windows()
    else:
        if not os.path.exists(uplink_filename):
            os.mkfifo(uplink_filename)

        uplink_fifo = os.open(uplink_filename, os.O_RDONLY | os.O_NONBLOCK)
        uplink_file = os.fdopen(uplink_fifo)

    print('[MC Interface] Reading from %s' % uplink_filename)

    while True:
        if is_windows:
            line = win32file.ReadFile(uplink_fifo, 64*1024)
            print(f"message: {line}")
        else:
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


