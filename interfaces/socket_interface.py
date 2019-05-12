import socket
import threading
import time
from socket import error as SocketError
import errno

from interfaces.mc_interface import MCInterface

HOST = '127.0.0.1'
PORT = 8700

class SocketInterface(MCInterface):

    def __init__(self, start_downlink, trigger_uplink):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.bind((HOST, PORT))
        self.socket.listen()

        self.connection = None

        server_thread = threading.Thread(target=self.listen_for_connections)
        server_thread.daemon = True
        server_thread.start()

        super().__init__(start_downlink, trigger_uplink)


    def listen_for_connections(self):
        """
        Let the server accept connections

        :return:
        """

        while True:
            connection, _addr = self.socket.accept()

            if self.connection:
                self.connection.close()

            self.connection = connection


    def on_downlinked_data(self, frame):
        """
        Called each time a frame of downlinked data comes in

        :param frame: Frame of download data. Should be a MIN frame
        :return:
        """

        self.log("Received downlinked data %s" % frame)

        if self.connection is None:
            self.log('MC is not listening')
        else:
            try:
                self.connection.sendall(frame.payload)
            except SocketError as e:
                if e.errno != errno.ECONNABORTED:
                    self.log('Connection lost')
                    self.connection.close()
                    self.connection = None
                else:
                    raise e


    def listen_for_data_to_uplink(self):
        """
        Creates a thread to poll the uplink fifo
        Calls all global callbacks

        :return:
        """

        uplink_file = None

        while True:
            if uplink_file is None and self.connection is not None:
                uplink_file = self.connection.makefile()

            if uplink_file is None:
                time.sleep(0.1)
                continue

            try:
                line = uplink_file.readline()
            except SocketError as e:
                if e.errno != errno.ECONNABORTED:
                    self.log('Connection lost')
                    self.connection.close()
                    self.connection = None
                    uplink_file = None
                    continue
                else:
                    raise e

            if not line:
                time.sleep(0.1)
                continue

            self.trigger_uplink(line.strip())
