#!/usr/bin/env python

import atexit
import os
import readline
import signal
from threading import Condition, Thread

from clingo import Control, Function, Number


class Connection:
    def __init__(self):
        self.condition = Condition()
        self.messages = []

    def receive(self, timeout=None):
        self.condition.acquire()
        while len(self.messages) == 0:
            self.condition.wait(timeout)
        message = self.messages.pop()
        self.condition.release()
        return message

    def send(self, message):
        self.condition.acquire()
        self.messages.append(message)
        self.condition.notify()
        self.condition.release()


class Controller:
    def __init__(self):
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
                ["more_pigeon_please", "less_pigeon_please", "solve", "exit"],
                text,
                state,
            )
        )
        atexit.register(readline.write_history_file, histfile)
        self.input = Connection()
        self.output = None

    def register_connection(self, connection):
        self.output = connection

    def interrupt(self, a, b):
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        self.output.send("interrupt")

    def run(self):
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
                print("Solving...")
                self.output.send("solve")
                signal.signal(signal.SIGINT, self.interrupt)
                # NOTE: we need a timeout to catch signals
                msg = self.input.receive(1000)
                print(msg)
            elif line == "exit":
                self.output.send("exit")
                break
            elif line in ["less_pigeon_please", "more_pigeon_please"]:
                self.output.send(line)
            else:
                print("unknown command: " + line)


class SolveThread(Thread):
    STATE_SOLVE = 1
    STATE_IDLE = 2
    STATE_EXIT = 3

    def __init__(self, connection):
        Thread.__init__(self)
        self.k = 0
        self.prg = Control()
        self.prg.load("client.lp")
        self.prg.ground([("pigeon", []), ("sleep", [Number(self.k)])])
        self.prg.assign_external(Function("sleep", [Number(self.k)]), True)
        self.state = SolveThread.STATE_IDLE
        self.input = Connection()
        self.output = connection

    def on_model(self, model):
        self.output.send("answer: " + str(model)),

    def on_finish(self, ret):
        self.output.send(
            "finish: " + str(ret) + (" (INTERRUPTED)" if ret.interrupted else "")
        )

    def handle_message(self, msg):
        if msg == "interrupt":
            self.state = SolveThread.STATE_IDLE
        elif msg == "exit":
            self.state = SolveThread.STATE_EXIT
        elif msg == "less_pigeon_please":
            self.prg.assign_external(Function("p"), False)
            self.state = SolveThread.STATE_IDLE
        elif msg == "more_pigeon_please":
            self.prg.assign_external(Function("p"), True)
            self.state = SolveThread.STATE_IDLE
        elif msg == "solve":
            self.state = SolveThread.STATE_SOLVE
        else:
            raise (RuntimeError("unexpected message: " + msg))

    def run(self):
        while True:
            if self.state == SolveThread.STATE_SOLVE:
                f = self.prg.solve(
                    on_model=self.on_model, on_finish=self.on_finish, async_=True
                )
            msg = self.input.receive()
            if self.state == SolveThread.STATE_SOLVE:
                f.cancel()
                ret = f.get()
            else:
                ret = None
            self.handle_message(msg)
            if self.state == SolveThread.STATE_EXIT:
                return
            elif ret is not None and not ret.unknown:
                self.k = self.k + 1
                self.prg.ground([("sleep", [Number(self.k)])])
                self.prg.release_external(Function("sleep", [Number(self.k - 1)]))
                self.prg.assign_external(Function("sleep", [Number(self.k)]), True)


ct = Controller()
st = SolveThread(ct.input)
ct.register_connection(st.input)

st.start()
ct.run()
st.join()
