import time
from interfaces.mc_interface import MCInterface

class ConsoleInterface(MCInterface):

    def on_downlinked_data(self, frame):
        print(str(frame.payload))

    def listen_for_data_to_uplink(self):
        print("Running console - type commands here:")

        while True:
            self.trigger_uplink(input(""))


if __name__ == "__main__":
    def fake_downlink(on_data):
        while True:
            on_data("test downlink")
            time.sleep(1)

    def trigger_uplink(message):
        print("Fake uplink %s" % message)

    ConsoleInterface(fake_downlink, trigger_uplink)
