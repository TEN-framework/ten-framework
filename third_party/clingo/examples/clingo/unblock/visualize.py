#!/usr/bin/python

import urwid
import sys

from clingo import SymbolType, Number, Function, Control

class Plan:
    def __init__(self, field, stone, target, move):
        mx           = min([x for (x, y) in field])
        my           = min([y for (x, y) in field])
        self.width   = max([x for (x, y) in field]) - mx + 1
        self.height  = max([y for (x, y) in field]) - my + 1
        self.field   = [ (x - mx, y - my) for (x, y) in field ]
        self.target  = [ (s, x - mx, y - my) for (s, x, y) in target ]
        self._stones = [ [ (s, d, x - mx, y - my, l) for (s, d, x, y, l) in stone ] ]
        self._moves  = [ ]
        self._locs   = [ ]

        for t in sorted(move.keys()):
            for (s, d, xy) in move[t]:
                xy -= (mx if d == "x" else my)
                self._moves.append((s, d, xy))
                self._stones.append([])
                for (ss, sd, sx, sy, sl) in self._stones[-2]:
                    if ss == s:
                        if d == "x": sx = xy
                        else       : sy = xy
                        self._locs.append((sd, sx, sy, sl))
                    self._stones[-1].append((ss, sd, sx, sy, sl))
        self._moves.append(None)
        self._locs.append(None)

    def loc(self, i):
        return self._locs[i]

    def move(self, i):
        return self._moves[i]

    def stones(self, i):
        return self._stones[i]

    def next(self, i):
        return i + 1 if i + 1 < len(self._stones) else i

    def prev(self, i):
        return i - 1 if i - 1 >= 0 else i

    def first(self):
        return 0

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

        for (s, d, x, y, l) in self.plan.stones(self.current):
            sx = 2 * x + 1
            sy = 2 * y + 1
            m  = self.plan.move(self.current)
            if m != None: m = m[0]

            for sd in range(0, 2 * l - 1):
               brd[sy + (sd if d == "y" else 0)][sx + (sd if d == "x" else 0)] = ('red', d) if s == m else d

        loc = self.plan.loc(self.current)
        if loc != None:
            (d, x, y, l) = loc
            x = 2 * x + 1
            y = 2 * y + 1
            for sd in range(0, 2 * l - 1):
                sx = x + (sd if d == "x" else 0)
                sy = y + (sd if d == "y" else 0)
                if brd[sy][sx] in [" ", "|", "-"]: brd[sy][sx] = ('blue', brd[sy][sx])

        for (s, x, y) in self.plan.target:
            s = brd[2 * y + 1][2 * x + 1]
            if isinstance(s, tuple): s = s[1]
            brd[2 * y + 1][2 * x + 1] = ("green", s)

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

    def run(self, plan):

        c = Board(plan)

        ba = urwid.Button("previous")
        bb = urwid.Button("quit")
        bc = urwid.Button("next")

        urwid.connect_signal(bb, 'click', self.exit)
        urwid.connect_signal(bc, 'click', c.next)
        urwid.connect_signal(ba, 'click', c.prev)

        sf = urwid.Text("")

        b = urwid.Columns([sf, ('fixed', len(bc.label) + 4, bc), ('fixed', len(ba.label) + 4, ba), ('fixed', len(bb.label) + 4, bb), sf], 1)
        f = urwid.Frame(urwid.Filler(c.display), None, b, 'footer')

        palette = [
            ('red', 'black', 'light red'),
            ('green', 'black', 'light green'),
            ('blue', 'black', 'light blue'), ]

        self.loop = urwid.MainLoop(f, palette)
        self.loop.run()

c = Control()
c.add("check", ["k"], "#external query(k).")
for f in sys.argv[1:]: c.load(f)
def make_on_model(field, stone, move, target):
    def on_model(m):
        for atom in m.symbols(atoms=True):
            if atom.name == "field" and len(atom.arguments) == 2:
                x, y = [n.number for n in atom.arguments]
                field.append((x, y))
            elif atom.name == "stone" and len(atom.arguments) == 5:
                s, d, x, y, l = [(n.number if n.type == SymbolType.Number else str(n)) for n in atom.arguments]
                stone.append((s, d, x, y, l))
            elif atom.name == "move" and len(atom.arguments) == 4:
                t, s, d, xy = [(n.number if n.type == SymbolType.Number else str(n)) for n in atom.arguments]
                move.setdefault(t, []).append((s, d, xy))
            elif atom.name == "target" and len(atom.arguments) == 3:
                s, x, y = [(n.number if n.type == SymbolType.Number else str(n)) for n in atom.arguments]
                target.append((s, x, y))
        return False
    return on_model

t, field, stone, move, target = 0, [], [], {}, []
on_model = make_on_model(field, stone, move, target)
c.ground([("base", [])])
while True:
    t += 1
    c.ground([("step", [Number(t)])])
    c.ground([("check", [Number(t)])])
    c.release_external(Function("query", [Number(t-1)]))
    c.assign_external(Function("query", [Number(t)]), True)
    if c.solve(on_model=on_model).satisfiable:
        break

MainWindow().run(Plan(field, stone, target, move))

