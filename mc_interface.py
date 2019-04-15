
def on_downlinked_data(frame):
    """
    Called each time a frame of downlinked data comes in

    :param frame:
    :return:
    """
    print("[MC Interface] Received downlinked data", frame)
