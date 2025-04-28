#!/usr/bin/env python
from clingo import SymbolType, Number, Function, Control
try:
    import Tkinter
except ImportError:
    import tkinter as Tkinter

# {{{1 class Board

class Board:
    def __init__(self):
        self.size           = 1
        self.blocked        = set()
        self.barriers       = set()
        self.targets        = set()
        self.pos            = dict()
        self.robots         = [{}]
        self.moves          = []
        self.current_target = None
        self.solution       = None

        ctl = Control()
        ctl.load("board.lp")
        ctl.ground([("base", [])])
        ctl.solve(on_model=self.__on_model)

    def __on_model(self, m):
        for atom in m.symbols(atoms=True):
            if atom.name == "barrier" and len(atom.arguments) == 4:
                x, y, dx, dy = [n.number for n in atom.arguments]
                self.blocked.add((x - 1     , y - 1     ,  dx,  dy))
                self.blocked.add((x - 1 + dx, y - 1     , -dx,  dy))
                self.blocked.add((x - 1     , y - 1 + dy,  dx, -dy))
                self.blocked.add((x - 1 + dx, y - 1 + dy, -dx, -dy))
                if dy == 0:
                    self.barriers.add(('west', x if dx == 1 else x - 1, y - 1))
                else:
                    self.barriers.add(('north', x - 1, y if dy == 1 else y - 1))
            elif atom.name == "dim" and len(atom.arguments) == 1:
                self.size = max(self.size, atom.arguments[0].number)
            elif atom.name == "available_target" and len(atom.arguments) == 4:
                c, s, x, y = [(n.number if n.type == SymbolType.Number else str(n)) for n in atom.arguments]
                self.targets.add((c, s, x - 1, y - 1))
            elif atom.name == "initial_pos" and len(atom.arguments) == 3:
                c, x, y = [(n.number if n.type == SymbolType.Number else str(n)) for n in atom.arguments]
                self.pos[c] = (x - 1, y - 1)
        for d in range(0, self.size):
            self.blocked.add((d            ,             0,  0, -1))
            self.blocked.add((d            , self.size - 1,  0,  1))
            self.blocked.add((0            ,             d, -1,  0))
            self.blocked.add((self.size - 1,             d,  1,  0))

    def move(self, robot, dx, dy):
        x, y = self.pos[robot]
        while (not (x, y, dx, dy) in self.blocked and
                not (x + dx, y + dy) in self.pos.values()):
            x += dx
            y += dy
        self.pos[robot] = (x, y)
        if (self.solution is not None and
                len(self.solution) > 0 and
                self.solution[0][0] == robot and
                self.solution[0][1] == dx and
                self.solution[0][2] == dy):
            self.solution.pop(0)
            if len(self.solution) == 0:
                self.solution = None
        else:
            self.solution = None

    def won(self):
        r, _, x, y = self.current_target
        return self.pos[r] == (x, y)

# {{{1 class Solver
# NOTE: it would be a nice gimmick to make the search interruptible

class Solver:
    def __init__(self, horizon=0):
        self.__horizon = horizon
        self.__prg = Control(['-t4'])
        self.__future = None
        self.__solution = None
        self.__assign = []

        self.__prg.load("board.lp")
        self.__prg.load("robots.lp")
        parts = [ ("base", [])
                , ("check", [Number(0)])
                , ("state", [Number(0)])
                ]
        for t in range(1, self.__horizon+1):
            parts.extend([ ("trans", [Number(t)])
                         , ("check", [Number(t)])
                         , ("state", [Number(t)])
                         ])
        self.__prg.ground(parts)
        self.__prg.assign_external(Function("horizon", [Number(self.__horizon)]), True)

    def __next(self):
        assert(self.__horizon < 30)
        self.__prg.assign_external(Function("horizon", [Number(self.__horizon)]), False)
        self.__horizon += 1
        self.__prg.ground([ ("trans", [Number(self.__horizon)])
                          , ("check", [Number(self.__horizon)])
                          , ("state", [Number(self.__horizon)])
                          ])
        self.__prg.assign_external(Function("horizon", [Number(self.__horizon)]), True)

    def start(self, board):
        self.__assign = []
        for robot, (x, y) in board.pos.items():
            self.__assign.append(Function("pos", [Function(robot), Number(x+1), Number(y+1), Number(0)]))
        self.__assign.append(Function("target",
            [ Function(board.current_target[0])
            , Number(board.current_target[2] + 1)
            , Number(board.current_target[3] + 1)
            ]))
        for x in self.__assign:
            self.__prg.assign_external(x, True)
        self.__solution = None
        self.__future = self.__prg.solve(on_model=self.__on_model, async_=True)

    def busy(self):
        if self.__future is None:
            return False
        if self.__future.wait(0):
            if self.__solution is None:
                self.__next()
                self.__future = self.__prg.solve(on_model=self.__on_model, async_=True)
                return True
            else:
                self.__future = None
                return False
        return True

    def stop(self):
        if self.__future is not None:
            self.__future.cancel()
            self.__future.wait()
            self.__future = None
            self.get()

    def get(self):
        solution = self.__solution
        self.__solution = None
        for x in self.__assign:
            self.__prg.assign_external(x, False)
        self.__assign = []
        return solution

    def __on_model(self, m):
        self.__solution = []
        for atom in m.symbols(atoms=True):
            if atom.name == "move" and len(atom.arguments) == 4:
                c, x, y, t = [(n.number if n.type == SymbolType.Number else str(n)) for n in atom.arguments]
                self.__solution.append((c, x, y, t))
        self.__solution.sort(key=lambda x: x[3])
        p = None
        i = 0
        for x in self.__solution:
            if p is not None and \
               p[0] == x[0]  and \
               p[1] == x[1]  and \
               p[2] == x[2]:
                break
            p = x
            i += 1
        del self.__solution[i:]

# {{{1 class Visualization

class Visualization:
    def __init__(self, master, board):
        self.__margin           = 20
        self.__tile_size        = 40
        self.__canvas_width     = None
        self.__canvas_height    = None
        self.__robot_images     = {}
        self.__target_images    = {}
        self.__solution_images  = []
        self.__direction_images = []
        self.__entered          = set()
        self.__slots            = {}
        self.__highlights       = {}
        self.__targets          = {}
        self.__moves            = {}
        self.__moves_short      = {}
        self.__robots           = {}
        self.__barriers         = {}
        self.__tiles            = []

        self.__canvas_width  = board.size * self.__tile_size + 2 * self.__margin
        self.__canvas_height = (1 + board.size) * self.__tile_size + 3 * self.__margin

        self.__canvas = Tkinter.Canvas(master, width=self.__canvas_width, height=self.__canvas_height)
        self.__canvas.pack()

        colors     = ['green', 'red', 'blue', 'yellow']
        shapes     = ['moon', 'sun', 'star', 'saturn']
        directions = [('north', 0, -1), ("east", 1, 0), ('south', 0, 1), ('west', -1, 0)]
        for orientation in ['left', 'right']:
            path = 'img/tile_{orientation}.gif'.format(orientation=orientation)
            self.__tiles.append(Tkinter.PhotoImage(file=path))
        for direction in ['north', 'west']:
            path = 'img/wall_{direction}.gif'.format(direction=direction)
            self.__barriers[direction] = (Tkinter.PhotoImage(file=path), -6, -6)
        for color in colors:
            path = 'img/robot_{color}.gif'.format(color=color)
            self.__robots[color] = Tkinter.PhotoImage(file=path)
            for shape in shapes:
                path = "img/{shape}_{color}.gif".format(shape=shape, color=color)
                self.__targets[(color, shape)] = Tkinter.PhotoImage(file=path)
            for (direction, dx, dy) in directions:
                path = "img/arrow_{color}_{direction}.gif".format(color=color, direction=direction)
                self.__moves[(color, dx, dy)] = Tkinter.PhotoImage(file=path)
                path = "img/move_{color}_{direction}.gif".format(color=color, direction=direction)
                self.__moves_short[(color, dx, dy)] = Tkinter.PhotoImage(file=path)
        for x in range(0, board.size):
            for y in range(0, board.size):
                self.__canvas.create_image(
                    self.__margin + self.__tile_size * x,
                    self.__margin + self.__tile_size * y,
                    anchor=Tkinter.NW,
                    image=self.__tiles[(x + y) % len(self.__tiles)])
        for (t, m, x, y) in board.targets:
            self.__target_images[(x, y)] = self.__canvas.create_image(
                self.__margin + self.__tile_size * x,
                self.__margin + self.__tile_size * y,
                anchor=Tkinter.NW,
                image=self.__targets[(t,m)])
            self.__canvas.itemconfig(
                self.__target_images[(x, y)],
                state=Tkinter.HIDDEN)
        for (r, (x, y)) in board.pos.items():
            self.__robot_images[r] = self.__canvas.create_image(
                self.__margin + self.__tile_size * x,
                self.__margin + self.__tile_size * y,
                anchor=Tkinter.NW,
                image=self.__robots[r])
        for (d, x, y) in board.barriers:
            (img, dx, dy) = self.__barriers[d]
            self.__canvas.create_image(
                self.__margin + self.__tile_size * x + dx,
                self.__margin + self.__tile_size * y + dy,
                anchor=Tkinter.NW,
                image=img)
        self.__solve_button = self.__canvas.create_text(
            board.size * self.__tile_size / 2 + self.__margin,
            (0.5 + board.size) * self.__tile_size + 2 * self.__margin,
            text="Solve!",
            activefill="blue",
            state=Tkinter.HIDDEN)
        self.__solving_text = self.__canvas.create_text(
            board.size * self.__tile_size / 2 + self.__margin,
            (0.5 + board.size) * self.__tile_size + 2 * self.__margin,
            text="Solving...",
            state=Tkinter.HIDDEN)
        self.__canvas.bind('<Motion>', self.__mouse_move_event)
        self.__canvas.bind('<Button-1>', self.__mouse_click_event)

    def __mouse_over(self, tag, mx, my):
        if self.__canvas.itemcget(tag, "state") == Tkinter.HIDDEN:
            return False
        x, y, xx, yy = self.__canvas.bbox(tag)
        return mx >= x and mx < xx and \
               my >= y and my < yy

    def __mouse_over_triangle(self, tag, mx, my, dx, dy):
        if self.__mouse_over(tag, mx, my):
            px, py = self.__canvas.coords(tag)
            px = (mx - px) / self.__tile_size
            py = (my - py) / self.__tile_size
            rx = px - py
            ry = px + py - 1
            if (dx - dy) * rx < 0 and (dx + dy) * ry < 0:
                return True
        return False

    def __mouse_click_event(self, e):
        clicked = set()
        for (x, y), t in self.__target_images.items():
            if self.__mouse_over(t, e.x, e.y):
                clicked.add(("target", (x, y)))
        for (t, val) in self.__direction_images:
            r, x, y, dx, dy = val
            if self.__mouse_over_triangle(t, e.x, e.y, dx, dy):
                clicked.add(("robot", val))
        if self.__mouse_over(self.__solve_button, e.x, e.y):
            clicked.add(("solve", None))

        for tag, val in clicked:
            for slot in self.__slots.get(tag, []):
                slot("click", val)

    def __mouse_move_event(self, e):
        entered = set()
        for ((x, y), t) in self.__target_images.items():
            if self.__mouse_over(t, e.x, e.y):
                entered.add(("target", (x, y)))
        for (t, val) in self.__direction_images:
            r, x, y, dx, dy = val
            if self.__mouse_over_triangle(t, e.x, e.y, dx, dy):
                entered.add(("robot", val))
        for (tag, val) in self.__entered - entered:
            for slot in self.__slots.get(tag, []):
                slot("leave", val)
        for (tag, val) in entered - self.__entered:
            for slot in self.__slots.get(tag, []):
                slot("enter", val)
        self.__entered = entered

    def highlight(self, x, y, active):
        if active and not (x, y) in self.__highlights:
            m  = 8
            xx = self.__margin + x * self.__tile_size + m
            yy = self.__margin + y * self.__tile_size + m
            self.__highlights[(x, y)] = self.__canvas.create_rectangle(
                (xx, yy, xx + self.__tile_size - 2 * m, yy + self.__tile_size - 2 * m),
                width=3,
                outline="blue")
        elif not active and (x, y) in self.__highlights:
            self.__canvas.delete(self.__highlights[(x, y)])
            del self.__highlights[(x, y)]

    def highlight_direction(self, x, y, dx, dy, active):
        if active and not (x, y, dx, dy) in self.__highlights:
            m  = 8
            xx = self.__margin + x * self.__tile_size + m
            yy = self.__margin + y * self.__tile_size + m
            xxx = xx + self.__tile_size - 2 * m
            yyy = yy + self.__tile_size - 2 * m
            cx = xx + (xxx - xx) / 2
            cy = yy + (yyy - yy) / 2
            if dx == -1: xx, xxx = xxx, xx
            if dy == -1: yy, yyy = yyy, yy
            if dy ==  0: xxx = xx
            if dx ==  0: yyy = yy
            self.__highlights[(x, y, dx, dy)] = self.__canvas.create_polygon(
                (xx, yy, xxx, yyy, cx, cy),
                width=3,
                outline="blue",
                fill="")
        elif not active and (x, y, dx, dy) in self.__highlights:
            self.__canvas.delete(self.__highlights[(x, y, dx, dy)])
            del self.__highlights[(x, y, dx, dy)]

    def clear_highlights(self):
        for p in self.__highlights.values():
            self.__canvas.delete(p)
        self.__highlights = {}

    def connect_target_event(self, slot):
        self.__slots.setdefault("target", []).append(slot)

    def connect_robot_event(self, slot):
        self.__slots.setdefault("robot", []).append(slot)

    def connect_solve_event(self, slot):
        self.__slots.setdefault("solve", []).append(slot)

    def update_board(self, board):
        self.clear_directions()
        for (r, (x, y)) in board.pos.items():
            ox, oy = self.__canvas.coords(self.__robot_images[r])
            self.__canvas.move(
                self.__robot_images[r],
                self.__margin + self.__tile_size * x - ox,
                self.__margin + self.__tile_size * y - oy)
            for dx, dy in [(1,0), (0,1), (-1,0), (0,-1)]:
                xx = x + dx
                yy = y + dy
                if not (x, y, dx, dy) in board.blocked and not (xx, yy) in board.pos.values():
                    self.__direction_images.append((
                        self.__canvas.create_image(
                            self.__margin + self.__tile_size * xx,
                            self.__margin + self.__tile_size * yy,
                            anchor=Tkinter.NW,
                            image=self.__moves_short[r, dx, dy]),
                        (r, x, y, dx, dy)))
        for tag in self.__solution_images:
            self.__canvas.delete(tag)
        self.__solution_images = []
        if board.solution is not None:
            i = 0
            for (r, x, y, _) in board.solution:
                self.__solution_images.append(
                    self.__canvas.create_image(
                        self.__margin + i * self.__tile_size,
                        2 * self.__margin + self.__tile_size * board.size,
                        anchor=Tkinter.NW,
                        image=self.__moves[(r, x, y)]))
                i += 1
    def enable_solve(self, board, state):
        self.__canvas.itemconfigure(self.__solve_button, state=Tkinter.NORMAL if state == "enabled" else Tkinter.HIDDEN)
        self.__canvas.itemconfigure(self.__solving_text, state=Tkinter.NORMAL if state == "busy" else Tkinter.HIDDEN)

    def update_target(self, board):
        for (t, m, x, y) in board.targets:
            self.__canvas.itemconfig(self.__target_images[(x, y)], state=Tkinter.NORMAL if board.current_target is None else Tkinter.HIDDEN)
        if board.current_target is not None:
            self.__canvas.itemconfig(self.__target_images[board.current_target[2], board.current_target[3]], state=Tkinter.NORMAL)

    def clear_directions(self):
        for (tag, _) in self.__direction_images:
            self.__canvas.delete(tag)
        self.__direction_images = []

# {{{1 Application

class Main:
    def __init__(self):
        self.__master = Tkinter.Tk()

        self.__board  = Board()
        self.__solver = Solver()
        self.__canvas = Visualization(self.__master, self.__board)

        #self.__master.bind("<Left>", self.__canvas.on_previous) # would be nice to have these two bindings as an undo/redo stack
        #self.__master.bind("<Right>", self.__canvas.on_next)
        self.__master.bind("<Escape>", lambda x: self.__master.quit())
        self.__canvas.update_target(self.__board)
        self.__canvas.connect_target_event(self.target_event)
        self.__canvas.connect_robot_event(self.robot_event)
        self.__canvas.connect_solve_event(self.solve_event)

    def target_event(self, event, pos):
        x, y = pos
        if self.__board.current_target is None:
            if event == "enter":
                self.__canvas.highlight(x, y, True)
            elif event == "leave":
                self.__canvas.highlight(x, y, False)
            elif event == "click":
                for t in self.__board.targets:
                    if t[2] == x and t[3] == y:
                        self.__board.current_target = t
                self.__canvas.update_target(self.__board)
                self.__update_board()

    def __update_board(self):
        self.__canvas.clear_highlights()
        self.__canvas.update_board(self.__board)
        won = self.__board.won()
        if won:
            self.__board.current_target = None
            self.__canvas.clear_directions()
            self.__canvas.update_target(self.__board)
        self.__canvas.enable_solve(self.__board, "enabled" if not won and self.__board.solution is None else "disabled")

    def robot_event(self, event, pos):
        r, x, y, dx, dy = pos
        if event == "enter":
            self.__canvas.highlight_direction(x+dx, y+dy, dx, dy, True)
        elif event == "leave":
            self.__canvas.highlight_direction(x+dx, y+dy, dx, dy, False)
        else:
            self.__solver.stop()
            self.__board.move(r, dx, dy)
            self.__update_board()

    def solve_event(self, event, ignore):
        self.__solver.start(self.__board)
        self.__canvas.enable_solve(self.__board, "busy")
        self.__master.after(500, self.timer_event)

    def timer_event(self):
        if self.__solver.busy():
            self.__master.after(500, self.timer_event)
        else:
            self.__board.solution = self.__solver.get()
            self.__update_board()

    def run(self):
        Tkinter.mainloop()

# {{{1 main

app = Main()
app.run()
