#!/usr/bin/env python

import atexit
import errno
import os
import random
import readline
import signal
import socket

histfile = os.path.join(os.path.expanduser("~"), ".controller")
try:
    readline.read_history_file(histfile)
except IOError:
    pass
readline.parse_and_bind("tab: complete")


def complete(commands, text, state):
    matches = []
    if state == 0:
        matches = [c for c in commands if c.startswith(text)]
    return matches[state] if state < len(matches) else None


readline.set_completer(
    lambda text, state: complete(
        ["more_pigeon_please", "less_pigeon_please", "solve", "exit"], text, state
    )
)
atexit.register(readline.write_history_file, histfile)


def handleMessages(conn):
    def interrupt(conn):
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        conn.sendall(b"interrupt\n")

    signal.signal(signal.SIGINT, lambda a, b: interrupt(conn))
    data = bytearray()
    while True:
        while True:
            try:
                data.extend(conn.recv(4096))
            except OSError as e:
                if e.errno != errno.EINTR:
                    raise
            except socket.error as e:
                code, msg = e
                if code != errno.EINTR:
                    raise
            else:
                break
        pos = data.find(b"\n")
        while pos >= 0:
            (msg, data) = data.split(b"\n", 1)
            msg = msg.decode()
            if msg.startswith("finish:"):
                msg = msg.split(":")
                print(msg[1] + (" (interrupted)" if len(msg) > 2 else ""))
                return
            elif msg.startswith("Answer:"):
                print(msg)
                pos = data.find(b"\n")
    signal.signal(signal.SIGINT, signal.SIG_IGN)


s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    for i in range(0, 10):
        try:
            p = random.randrange(1000, 60000)
            s.bind(("", p))
        except OSError as e:
            if e.errno != errno.EINTR:
                raise
        except socket.error as e:
            code, msg = e
            if code != errno.EADDRINUSE:
                raise
            continue
        else:
            with open(".controller.PORT", "w") as f:
                f.write(str(p))
            print("waiting for connections...")
            break
        raise "no port found"
    s.listen(1)
    conn, addr = s.accept()
    print("")
    print("this prompt accepts the following commands:")
    print("  solve              - start solving")
    print("  exit/EOF           - terminate the solver")
    print("  Ctrl-C             - interrupt current search")
    print("  less_pigeon_please - select an easy problem")
    print("  more_pigeon_please - select a difficult problem")
    print("")

    pyInt = signal.getsignal(signal.SIGINT)
    while True:
        signal.signal(signal.SIGINT, pyInt)
        try:
            try:
                input_ = raw_input
            except NameError:
                input_ = input
            line = input_("> ")
            signal.signal(signal.SIGINT, signal.SIG_IGN)
        except EOFError:
            signal.signal(signal.SIGINT, signal.SIG_IGN)
            line = "exit"
            print(line)
        except KeyboardInterrupt:
            signal.signal(signal.SIGINT, signal.SIG_IGN)
            print
            continue
        if line == "solve":
            conn.sendall(b"solve\n")
            handleMessages(conn)
        elif line == "exit":
            conn.sendall(b"exit\n")
            break
        elif line in ["less_pigeon_please", "more_pigeon_please"]:
            conn.sendall(line.encode() + b"\n")
        else:
            print("unknown command: " + line)
except KeyboardInterrupt:
    raise
finally:
    s.close()
