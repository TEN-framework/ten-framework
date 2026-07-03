#!/usr/bin/env python

import atexit
import os
import readline
import signal
from threading import Condition

from clingo import Control, Function, Number


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
        self.solving = False
        self.condition = Condition()

    def register_solver(self, solver):
        self.solver = solver

    def interrupt(self, a, b):
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        self.solver.stop()

    def on_finish(self, ret):
        self.message = (
            "finish: " + str(ret) + (" (INTERRUPTED)" if ret.interrupted else "")
        )
        self.condition.acquire()
        self.solving = False
        self.condition.notify()
        self.condition.release()

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
                self.solving = True
                self.solver.start(self.on_finish)
                signal.signal(signal.SIGINT, self.interrupt)
                self.condition.acquire()
                while self.solving:
                    # NOTE: we need a timeout to catch signals
                    self.condition.wait(1000)
                self.condition.release()
                self.solver.finish()
                for model in self.solver.models:
                    print("model: " + model)
                print(self.message)
            elif line == "exit":
                break
            elif line == "less_pigeon_please":
                self.solver.set_more_pigeon(False)
            elif line == "more_pigeon_please":
                self.solver.set_more_pigeon(True)
            else:
                print("unknown command: " + line)


class Solver:
    def __init__(self):
        self.k = 0
        self.prg = Control()
        self.prg.load("client.lp")
        self.prg.ground([("pigeon", []), ("sleep", [Number(self.k)])])
        self.prg.assign_external(Function("sleep", [Number(self.k)]), True)
        self.ret = None
        self.models = []

    def on_model(self, model):
        self.models.append(str(model))

    def start(self, on_finish):
        if self.ret is not None and not self.ret.unknown():
            self.k = self.k + 1
            self.prg.ground([("sleep", [self.k])])
            self.prg.release_external(Function("sleep", [Number(self.k - 1)]))
            self.prg.assign_external(Function("sleep", [Number(self.k)]), True)
        self.future = self.prg.solve(
            on_model=self.on_model, on_finish=on_finish, async_=True
        )

    def stop(self):
        self.future.cancel()

    def finish(self):
        ret = self.future.get()
        return ret

    def set_more_pigeon(self, more):
        self.prg.assign_external(Function("p"), more)


st = Solver()
ct = Controller()
ct.register_solver(st)
ct.run()
