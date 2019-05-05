import threading


def run_console(start_downlink, trigger_uplink):
    downlink_listener_thread = threading.Thread(target=start_downlink)
    downlink_listener_thread.daemon = True
    downlink_listener_thread.start()

    while True:
        trigger_uplink(input(""))


if __name__ == "__main__":
    def fake_downlink():
        pass

    def trigger_uplink(message):
        print("Fake uplink %s" % message)

    run_console(fake_downlink, trigger_uplink)
