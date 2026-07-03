#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import socket
import time


def is_app_started(ip: str, port: int, timeout=10):
    duration = 0

    while duration < timeout:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((ip, int(port)))
            print("The app server starts in %d seconds" % duration)

            return True, s
        except Exception as e:
            print("Can not connect to app server", e)

        duration += 1
        time.sleep(1)

    # The app server running behind the msgpack protocol will be GC when the
    # connection is closed. So we can _NOT_ shutdown the socket now, and the
    # socket should be closed after the closure of the client.
    return False, None
