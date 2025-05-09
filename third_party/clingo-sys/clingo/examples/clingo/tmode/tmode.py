from copy import copy
from sys import exit, stdout
from textwrap import dedent

from clingo import Function, Number, SymbolType, ast, clingo_main
from clingo.application import Application


class TermTransformer(ast.Transformer):
    def __init__(self, parameter):
        self.parameter = parameter

    def __get_param(self, name, location):
        n = name.replace("'", "")
        primes = len(name) - len(n)
        param = ast.SymbolicTerm(location, self.parameter)
        if primes > 0:
            param = ast.BinaryOperation(
                location,
                ast.BinaryOperator.Minus,
                param,
                ast.SymbolicTerm(location, Number(primes)),
            )
        return n, param

    def visit_Function(self, term):
        name, param = self.__get_param(term.name, term.location)
        term = term.update(name=name)
        term.arguments.append(param)
        return term

    def visit_SymbolicTerm(self, term):
        # this function is not necessary if gringo's parser is used
        # but this case could occur in a valid AST
        raise RuntimeError("not implemented")


class ProgramTransformer(ast.Transformer):
    def __init__(self, parameter):
        self.final = False
        self.parameter = parameter
        self.term_transformer = TermTransformer(parameter)

    def visit(self, x, *args, **kwargs):
        ret = super().visit(x, *args, **kwargs)
        if self.final and hasattr(ret, "body"):
            if x is ret:
                ret = copy(x)
            loc = ret.location
            fun = ast.Function(
                loc, "finally", [ast.SymbolicTerm(loc, self.parameter)], False
            )
            atm = ast.SymbolicAtom(fun)
            lit = ast.Literal(loc, ast.Sign.NoSign, atm)
            ret.body.append(lit)
        return ret

    def visit_SymbolicAtom(self, atom):
        return atom.update(symbol=self.term_transformer(atom.symbol))

    def visit_Program(self, prg):
        self.final = prg.name == "final"
        prg = copy(prg)
        if self.final:
            prg.name = "static"
        prg.parameters.append(ast.Id(prg.location, self.parameter.name))
        return prg

    def visit_ShowSignature(self, sig):
        return sig.update(arity=sig.arity + 1)

    def visit_ProjectSignature(self, sig):
        return sig.update(arity=sig.arity + 1)


class TModeApp(Application):
    def __init__(self):
        self._imin = 0
        self._imax = None
        self._istop = "SAT"
        self._horizon = 0

    def _parse_imin(self, value):
        try:
            self._imin = int(value)
        except ValueError:
            return False
        return self._imin >= 0

    def _parse_imax(self, value):
        if value.upper() in ("INF", "INFINITY"):
            self._imax = None
            return True
        try:
            self._imax = int(value)
        except ValueError:
            return False
        return self._imax >= 0

    def _parse_istop(self, value):
        self._istop = value.upper()
        return self._istop in ["SAT", "UNSAT", "UNKNOWN"]

    def register_options(self, options):
        group = "Incremental Options"
        options.add(
            group,
            "imin",
            "Minimum number of solving steps [0]",
            self._parse_imin,
            argument="<n>",
        )
        options.add(
            group,
            "imax",
            "Maximum number of solving steps [infinity]",
            self._parse_imax,
            argument="<n>",
        )
        options.add(
            group,
            "istop",
            dedent(
                """\
            Stop criterion [sat]
                  <arg>: {sat|unsat|unknown}"""
            ),
            self._parse_istop,
        )

    def print_model(self, model, printer):
        table = {}
        for sym in model.symbols(shown=True):
            if sym.type == SymbolType.Function and len(sym.arguments) > 0:
                table.setdefault(sym.arguments[-1], []).append(
                    Function(sym.name, sym.arguments[:-1])
                )
        for step, symbols in sorted(table.items()):
            stdout.write(" State {}:".format(step))
            sig = None
            for sym in sorted(symbols):
                if (sym.name, len(sym.arguments)) != sig:
                    stdout.write("\n ")
                    sig = (sym.name, len(sym.arguments))
                stdout.write(" {}".format(sym))
            stdout.write("\n")

    def _main(self, ctl):
        step, ret = 0, None
        while (self._imax is None or step < self._imax) and (
            step == 0
            or step < self._imin
            or (
                (self._istop == "SAT" and not ret.satisfiable)
                or (self._istop == "UNSAT" and not ret.unsatisfiable)
                or (self._istop == "UNKNOWN" and not ret.unknown)
            )
        ):
            parts = []
            parts.append(("base", [Number(step)]))
            parts.append(("static", [Number(step)]))
            if step > 0:
                ctl.release_external(Function("finally", [Number(step - 1)]))
                parts.append(("dynamic", [Number(step)]))
            else:
                parts.append(("initial", [Number(0)]))
            ctl.ground(parts)
            ctl.assign_external(Function("finally", [Number(step)]), True)
            ret, step = ctl.solve(), step + 1

    def main(self, ctl, files):
        with ast.ProgramBuilder(ctl) as bld:
            ptf = ProgramTransformer(Function("__t"))
            ast.parse_files(files, lambda stm: bld.add(ptf(stm)))
        ctl.add("initial", ["t"], "initially(t).")
        ctl.add("static", ["t"], "#external finally(t).")
        self._main(ctl)


exit(clingo_main(TModeApp()))
