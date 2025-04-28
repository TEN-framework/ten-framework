#!/usr/bin/env python

import json
import sys
import argparse

from clingo import Control, Number

class App:
    def __init__(self, args):
        self.control = Control()
        self.args = args
        self.horizon = 0
        self.objects = 0
        self.end     = None

    def show(self, model):
        if not self.args.quiet:
            print("Model: {}".format(model))

    def ground(self, kind):
        count = self.objects + self.horizon + 1
        parts = [("expand", [Number(count)])]

        if self.args.scratch and count > 1:
            self.control = Control()
            for source in self.args.file: self.control.load(source)
            for i in range(0, self.objects): parts.append(("object", [Number(i + 1, count)]))
            for i in range(0, self.horizon): parts.append(("horizon", [Number(i + 1, count)]))

        if self.args.scratch or count == 1:
            for option in self.args.option:
                setattr(self.control.configuration, option[0], option[1])
            parts.append(("base", []))

        if kind:
            self.objects += 1
            parts.append(("object", [Number(self.objects), Number(count)]))
        else:
            self.horizon += 1
            parts.append(("horizon", [Number(self.horizon), Number(count)]))

        if self.args.verbose:
             print("")
             print("Objects: {}".format(Number(self.objects)))
             print("Horizon: {}".format(Number(self.horizon)))

        self.control.ground(parts)

        if self.args.verbose:
             print("Solving: {}".format(count))

    def run(self):
        for source in self.args.file:
            self.control.load(source)
        if self.args.maxobj is None:
            self.end = self.control.get_const("n").number
        else:
            self.end = self.args.maxobj

        while self.objects < self.end:
            self.ground(True)
            while True:
                ret = self.control.solve(on_model=self.show)
                if self.args.stats:
                    args = {"sort_keys": True, "indent": 0, "separators": (',', ': ')}
                    stats = {}
                    for x in ["step", "enumerated", "time_cpu", "time_solve", "time_sat", "time_unsat", "time_total"]:
                        stats[x] = self.control.statistics[x]
                    for x in ["lp", "ctx", "solvers"]:
                        for y in self.control.statistics[x]:
                            stats[y] = self.control.statistics[x][y]
                    print(json.dumps(stats, *args))
                if ret.satisfiable:
                    break
                self.ground(False)

parser = argparse.ArgumentParser(description="Gradually expand logic programs.", epilog="""Example: main.py -x -q -s -v -m 42 -o solve.models 0 encoding.lp instance.lp""")

parser.add_argument("-x", "--scratch", action='store_true', help="start each step from scratch (single-shot solving)")
parser.add_argument("-q", "--quiet", action='store_true', help="do not print models")
parser.add_argument("-s", "--stats", action='store_true', help="print solver statistics")
parser.add_argument("-v", "--verbose", action='store_true', help="print progress information")
parser.add_argument("-m", "--maxobj", type=int, metavar="NUM", default=None, help="maximum number of introduced objects")
parser.add_argument("-o", "--option", nargs=2, metavar=("OPT", "VAL"), action="append", default=[], help="set sover options")
parser.add_argument("file", nargs="*", default=[], help="gringo source files")

args = parser.parse_args()
if args.maxobj is not None and args.maxobj < 1:
    parser.error("maximum number of objects must be positive")

App(args).run()

