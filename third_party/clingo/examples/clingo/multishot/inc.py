'''
Example implementing an iclingo-like application.
'''

import sys
from typing import cast, Any, Callable, Optional, Sequence

from clingo.application import clingo_main, Application, ApplicationOptions
from clingo.control import Control
from clingo.solving import SolveResult
from clingo.symbol import Function, Number


class IncConfig:
    '''
    Configuration object for incremental solving.
    '''
    imin: int
    imax: Optional[int]
    istop: str

    def __init__(self):
        self.imin = 1
        self.imax = None
        self.istop = "SAT"


def parse_int(conf: Any,
              attr: str,
              min_value: Optional[int] = None,
              optional: bool = False) -> Callable[[str], bool]:
    '''
    Returns a parser for integers.

    The parser stores its result in the `attr` attribute (given as string) of
    the `conf` object. The parser can be configured to only accept integers
    having a minimum value and also to treat value `"none"` as `None`.
    '''
    def parse(sval: str) -> bool:
        if optional and sval == "none":
            value = None
        else:
            value = int(sval)
            if min_value is not None and value < min_value:
                raise RuntimeError("value too small")
        setattr(conf, attr, value)
        return True
    return parse


def parse_stop(conf: Any, attr: str) -> Callable[[str], bool]:
    '''
    Returns a parser for `istop` values.
    '''
    def parse(sval: str) -> bool:
        if sval not in ("SAT", "UNSAT", "UNKNOWN"):
            raise RuntimeError("invalid value")
        setattr(conf, attr, sval)
        return True
    return parse


class IncApp(Application):
    '''
    The example application implemeting incremental solving.
    '''
    program_name: str = "inc-example"
    version: str = "1.0"
    _conf: IncConfig

    def __init__(self):
        self._conf = IncConfig()

    def register_options(self, options: ApplicationOptions):
        '''
        Register program options.
        '''
        group = "Inc-Example Options"

        options.add(
            group, "imin",
            "Minimum number of steps [{}]".format(self._conf.imin),
            parse_int(self._conf, "imin", min_value=0),
            argument="<n>")

        options.add(
            group, "imax",
            "Maximum number of steps [{}]".format(self._conf.imax),
            parse_int(self._conf, "imax", min_value=0, optional=True),
            argument="<n>")

        options.add(
            group, "istop",
            "Stop criterion [{}]".format(self._conf.istop),
            parse_stop(self._conf, "istop"))

    def main(self, ctl: Control, files: Sequence[str]):
        '''
        The main function implementing incremental solving.
        '''
        if not files:
            files = ["-"]
        for file_ in files:
            ctl.load(file_)
        ctl.add("check", ["t"], "#external query(t).")

        conf = self._conf
        step = 0
        ret: Optional[SolveResult] = None

        while ((conf.imax is None or step < conf.imax) and
               (ret is None or step < conf.imin or (
                   (conf.istop == "SAT" and not ret.satisfiable) or
                   (conf.istop == "UNSAT" and not ret.unsatisfiable) or
                   (conf.istop == "UNKNOWN" and not ret.unknown)))):
            parts = []
            parts.append(("check", [Number(step)]))
            if step > 0:
                ctl.release_external(Function("query", [Number(step - 1)]))
                parts.append(("step", [Number(step)]))
            else:
                parts.append(("base", []))
            ctl.ground(parts)

            ctl.assign_external(Function("query", [Number(step)]), True)
            ret, step = cast(SolveResult, ctl.solve()), step + 1


clingo_main(IncApp(), sys.argv[1:])
