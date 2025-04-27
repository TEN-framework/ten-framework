#!/usr/bin/python

import urwid
import sys
from clingo import SymbolType, Number, Function, clingo_main

class Board:

    def __init__(self, plan):
        self.display = urwid.Text("", align='center')
        self.plan    = plan
        self.current = plan.first()
        self.update()

    def next(self, button):
        self.current = self.plan.next(self.current)
        self.update()

    def prev(self, button):
        self.current = self.plan.prev(self.current)
        self.update()

    def update(self):
        brd = [[" "] * (self.plan.width * 2 + 1) for _ in range(0, (self.plan.height * 2 + 1))]

        for (x, y) in self.plan.field:
            brd[2 * y + 1][2 * x + 0] = "|"
            brd[2 * y + 1][2 * x + 2] = "|"

        for (x, y) in self.plan.field:
            brd[2 * y + 0][2 * x + 0] = "-"
            brd[2 * y + 0][2 * x + 1] = "-"
            brd[2 * y + 0][2 * x + 2] = "-"
            brd[2 * y + 2][2 * x + 0] = "-"
            brd[2 * y + 2][2 * x + 1] = "-"
            brd[2 * y + 2][2 * x + 2] = "-"

        for (x, y) in self.plan.steps(self.current):
            brd[2 * y + 1][2 * x + 1] = "o"

        j = self.plan.jumped(self.current)
        if j != None:
            (x, y) = j
            brd[2 * y  + 1][2 * x  + 1] = ("blue", "o")


        j = self.plan.jump(self.current)
        if j != None:
            (x, y, xx, yy) = j
            brd[2 * y  + 1][2 * x  + 1] = ("red",   "o")
            brd[2 * yy + 1][2 * xx + 1] = ("green", " ")

        markup = []
        for row in brd:
            markup.extend(row)
            markup.append("\n")

        self.display.set_text(markup)

class MainWindow:
    def __init__(self):
           self.loop = None

    def exit(self, button):
        raise urwid.ExitMainLoop()

    def quit(self, button):
        raise KeyboardInterrupt()

    def run(self, plan):

        c = Board(plan)

        ba = urwid.Button("previous")
        bb = urwid.Button("done")
        bc = urwid.Button("next")
        bd = urwid.Button("quit")

        urwid.connect_signal(bb, 'click', self.exit)
        urwid.connect_signal(bd, 'click', self.quit)
        urwid.connect_signal(bc, 'click', c.next)
        urwid.connect_signal(ba, 'click', c.prev)

        sf = urwid.Text("")

        b = urwid.Columns([sf, ('fixed', len(bc.label) + 4, bc), ('fixed', len(ba.label) + 4, ba), ('fixed', len(bb.label) + 4, bb), ('fixed', len(bd.label) + 4, bd), sf], 1)
        f = urwid.Frame(urwid.Filler(c.display), None, b, 'footer')

        palette = [
            ('red', 'black', 'light red'),
            ('green', 'black', 'light green'),
            ('blue', 'black', 'light blue'), ]

        self.loop = urwid.MainLoop(f, palette)
        self.loop.run()

class Plan:
    def __init__(self, field, init, jumps):
        mx          = min(x for (x, y) in field)
        my          = min(y for (x, y) in field)
        self.width  = max(x for (x, y) in field) - mx + 1
        self.height = max(y for (x, y) in field) - my + 1
        self.field  = [ (x - mx, y - my) for (x, y) in field ]

        pjumps = {}
        for (t, k) in jumps.items(): pjumps[t] = [ (x - mx, y - my, xx - mx, yy - my) for (x, y, xx, yy) in k ]
        self._jumps = []
        self._steps = []
        self._steps.append([ (x - mx, y - my) for (x, y) in init ])

        for t in sorted(pjumps.keys()):
            for (x, y, xx, yy) in pjumps[t]:
                self._jumps.append((x, y, xx, yy))
                self._steps.append(self._steps[-1][:])
                self._steps[-1].append((xx, yy))
                self._steps[-1].remove((x, y))
                self._steps[-1].remove(((x + xx) // 2, (y + yy) // 2))

    def jump(self, i):
        return self._jumps[i] if i < len(self._jumps) else None

    def jumped(self, i):
        return None if i <= 0 else self.jump(i - 1)[2:]

    def steps(self, i):
        return self._steps[i]

    def next(self, i):
        return i + 1 if i + 1 < len(self._steps) else i

    def prev(self, i):
        return i - 1 if i - 1 >= 0 else i

    def first(self):
        return 0

class Application:
    def __init__(self, name):
        self.program_name = name
        self.version = "1.0"

    def __on_model(self, model):
        sx = { "east": 2, "west": -2, "north":  0, "south": 0 }
        sy = { "east": 0, "west":  0, "north": -2, "south": 2 }

        field, init, jumps = [], [], {}

        for atom in model.symbols(atoms=True):
            if atom.name == "field" and len(atom.arguments) == 2:
                x, y = (n.number for n in atom.arguments)
                field.append((x, y))
            elif atom.name == "stone" and len(atom.arguments) == 2:
                x, y = (n.number for n in atom.arguments)
                init.append((x, y))
            elif atom.name == "jump" and len(atom.arguments) == 4:
                ox, oy, d, t = ((n.number if n.type == SymbolType.Number else str(n)) for n in atom.arguments)
                jumps.setdefault(t, []).append((ox, oy, ox + sx[d], oy + sy[d]))

        try:
            MainWindow().run(Plan(field, init, jumps))
            return True
        except KeyboardInterrupt:
            return False

    def main(self, prg, files):
        for f in files:
            prg.load(f)
        prg.add("check", ["k"], "#external query(k).")

        t = 0
        sat = False
        prg.ground([("base", [])])
        while not sat:
            t += 1
            prg.ground([("step", [Number(t)])])
            prg.ground([("check", [Number(t)])])
            prg.release_external(Function("query", [Number(t-1)]))
            prg.assign_external(Function("query", [Number(t)]), True)
            sat = prg.solve(on_model=self.__on_model).satisfiable

sys.exit(int(clingo_main(Application("visualize"), sys.argv[1:])))
