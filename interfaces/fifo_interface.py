import os
import time

from interfaces.mc_interface import MCInterface

UPLINK_FILENAME = 'uplink.fifo'
DOWNLINK_FILENAME = 'downlink.fifo'

class FIFOInterface(MCInterface):

    def __init__(self, start_downlink, trigger_uplink):
        self.downlink_fifo = None

        super().__init__(start_downlink, trigger_uplink)


    def on_downlinked_data(self, frame):
        """
        Called each time a frame of downlinked data comes in

        :param frame: Frame of download data. Should be a MIN frame
        :return:
        """

        self.log("Received downlinked data %s" % frame)

        if self.downlink_fifo is None:
            self.create_downlink_fifo()

        if self.downlink_fifo is not None:
            try:
                os.write(self.downlink_fifo, frame.payload)
            except OSError as e:
                if e.errno == 32:
                    self.log('Downlink pipe broken; closing')
                    os.close(self.downlink_fifo)
                    self.downlink_fifo = None
                else:
                    raise e


    def create_downlink_fifo(self):
        try:
            self.downlink_fifo = os.open(DOWNLINK_FILENAME, os.O_WRONLY | os.O_NONBLOCK)
        except OSError as e:
            if e.errno == 6 or e.errno == 2:
                self.log('Failed to open named pipe; MC is not listening')
            else:
                raise e

        if self.downlink_fifo is not None:
            self.log("Outputting data to %s" % DOWNLINK_FILENAME)


    def listen_for_data_to_uplink(self):
        """
        Creates a thread to poll the uplink fifo
        Calls all global callbacks

        :return:
        """

        if not os.path.exists(UPLINK_FILENAME):
            os.mkfifo(UPLINK_FILENAME)

        uplink_fifo = os.open(UPLINK_FILENAME, os.O_RDONLY | os.O_NONBLOCK)
        uplink_file = os.fdopen(uplink_fifo)

        self.log('Reading from %s' % UPLINK_FILENAME)

        while True:
            line = uplink_file.readline()

            if not line:
                time.sleep(0.1)
                continue

            self.trigger_uplink(line)
