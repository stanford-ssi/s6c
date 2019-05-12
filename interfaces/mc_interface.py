import threading

class MCInterface:

    def __init__(self, start_downlink, trigger_uplink):
        self.trigger_uplink = trigger_uplink

        downlink_listener_thread = threading.Thread(target=start_downlink, args=(self.on_downlinked_data,))
        downlink_listener_thread.daemon = True
        downlink_listener_thread.start()

        self.listen_for_data_to_uplink()

    def log(self, message):
        print('[MC Interface] %s' % message)

    def on_downlinked_data(self, frame):
        raise Exception("Abstract method on_downlinked_data not over-ridden")


    def listen_for_data_to_uplink(self):
        raise Exception("Abstract method listen_for_data_to_uplink not over-ridden")

