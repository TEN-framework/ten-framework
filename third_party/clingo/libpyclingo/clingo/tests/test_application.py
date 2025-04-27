"""
Test clingo's Application class.
"""

from typing import Any, Callable, List, Sequence, Tuple
from unittest import TestCase
from tempfile import NamedTemporaryFile
from multiprocessing import Process, Queue
import os
import re

from .util import _MCB
from ..core import MessageCode
from ..application import Application, ApplicationOptions, Flag, clingo_main


class TestApp(Application):
    """
    Test application covering most of the Application related API.

    Note that I did not find a nice way to test model printing.
    """

    _queue: Queue
    program_name = "test"
    version = "1.2.3"
    message_limit = 17

    def __init__(self, queue: Queue):
        self._queue = queue
        self._flag = Flag()

    def _parse_test(self, value):
        self._queue.put(("parse", value))
        return True

    def register_options(self, options: ApplicationOptions) -> None:
        self._queue.put("register")
        group = "Clingo.Test"
        options.add(group, "test", "test description", self._parse_test)
        options.add_flag(group, "flag", "test description", self._flag)

    def validate_options(self) -> bool:
        self._queue.put("validate")
        self._queue.put(("flag", self._flag.flag))
        return True

    def logger(self, code: MessageCode, message: str) -> None:
        self._queue.put((code, re.sub("^.*:(?=[0-9]+:)", "", message)))

    def main(self, control, files):
        self._queue.put("main")
        for file_ in files:
            control.load(file_)
        control.ground([("base", [])])
        mcb = _MCB()
        control.solve(on_model=mcb.on_model)
        self._queue.put(
            ("models", [[str(sym) for sym in model] for model in mcb.models])
        )


def _run_process(
    app: Callable[[Queue], Application], program: str, queue: Queue, args: Sequence[str]
) -> None:
    """
    Run clingo application with given program and intercept results.
    """
    with NamedTemporaryFile(mode="wt", delete=False) as fp:
        name = fp.name
        fp.write(program)
    try:
        # Note: The multiprocess module does not allow for intercepting the
        # output. Thus, the output is simply disabled and we use the Queue
        # class to communicate results.
        ret = clingo_main(app(queue), (name, "--outf=3") + tuple(args))
        queue.put(int(ret))
        queue.close()
    finally:
        os.unlink(name)


AppResult = Tuple[int, List[Any]]


def run_app(
    app: Callable[[Queue], Application], program: str, *args: Sequence[str]
) -> AppResult:
    """
    Run clingo application in subprocess via multiprocessing module.
    """
    q: Queue
    q = Queue()
    p = Process(target=_run_process, args=(app, program, q, tuple(args)))

    p.start()
    seq: List[Any]
    seq, ret = [], -1
    while True:
        ret = q.get()
        if isinstance(ret, int):
            status = ret
            break
        seq.append(ret)
    p.join()
    q.close()

    return status, seq


class TestApplication(TestCase):
    """
    Tests for clingo's application class.
    """

    def test_app(self):
        """
        Test application.
        """
        ret, seq = run_app(TestApp, "1 {a; b; c(1/0)}.", "0", "--test=x", "--flag")
        self.assertEqual(ret, 30)
        self.assertEqual(
            seq,
            [
                "register",
                ("parse", "x"),
                "validate",
                ("flag", True),
                "main",
                (
                    MessageCode.OperationUndefined,
                    "1:12-15: info: operation undefined:\n  (1/0)\n",
                ),
                ("models", [["a"], ["a", "b"], ["b"]]),
            ],
        )
