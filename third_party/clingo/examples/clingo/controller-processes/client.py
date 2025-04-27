import socket
import os
import errno
from clingo import Control, Function, Number

class Receiver:
    def __init__(self, conn):
        self.conn = conn
        self.data = bytearray()
    def readline(self):
        pos = self.data.find(b"\n")
        while pos < 0:
            while True:
                try: self.data.extend(self.conn.recv(4096))
                except OSError as e:
                    if e.errno != errno.EINTR: raise
                except socket.error as e:
                    code, msg = e
                    if code != errno.EINTR: raise
                else: break
            pos = self. data.find(b"\n")
        msg = self.data[:pos]
        self.data = self.data[pos+1:]
        return msg

class States:
    SOLVE = 1
    IDLE  = 2

def main(prg):
    with open(".controller.PORT", "r") as f:
        p = int(f.read())
    os.remove(".controller.PORT")
    conn = socket.create_connection(("127.0.0.1", p))
    try:
        recv  = Receiver(conn)
        state = States.IDLE
        k     = 0
        prg.ground([("pigeon", []), ("sleep",  [Number(k)])])
        prg.assign_external(Function("sleep", [Number(k)]), True)
        while True:
            if state == States.SOLVE:
                f = prg.solve(
                    on_model  = lambda model: conn.sendall(b"Answer: " + str(model).encode() + b"\n"),
                    on_finish = lambda ret:   conn.sendall(b"finish:" + str(ret).encode() + (b":INTERRUPTED" if ret.interrupted else b"") + b"\n"), async_=True)
            msg = recv.readline().decode()
            if state == States.SOLVE:
                f.cancel()
                ret = f.get()
            else:
                ret = None
            if msg == "interrupt":
                state = States.IDLE
            elif msg == "exit":
                return
            elif msg == "less_pigeon_please":
                prg.assign_external(Function("p"), False)
                state = States.IDLE
            elif msg == "more_pigeon_please":
                prg.assign_external(Function("p"), True)
                state = States.IDLE
            elif msg == "solve":
                state = States.SOLVE
            else: raise(RuntimeError("unexpected message: " + msg))
            if ret is not None and not ret.unknown:
                k = k + 1
                prg.ground([("sleep", [Number(k)])])
                prg.release_external(Function("sleep", [Number(k-1)]))
                prg.assign_external(Function("sleep", [Number(k)]), True)
    finally:
        conn.close()

prg = Control()
prg.load("client.lp")
main(prg)

