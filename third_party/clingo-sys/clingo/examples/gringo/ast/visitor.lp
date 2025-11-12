#script (python)

import clingo
import clingo.ast

class Transformer:
    def visit_children(self, x, *args, **kwargs):
        for key in x.child_keys:
            setattr(x, key, self.visit(getattr(x, key), *args, **kwargs))
        return x

    def visit(self, x, *args, **kwargs):
        if isinstance(x, clingo.ast.AST):
            attr = "visit_" + str(x.type)
            if hasattr(self, attr):
                return getattr(self, attr)(x, *args, **kwargs)
            else:
                return self.visit_children(x, *args, **kwargs)
        elif isinstance(x, list):
            return [self.visit(y, *args, **kwargs) for y in x]
        elif x is None:
            return x
        else:
            raise TypeError("unexpected type")

class TermTransformer(Transformer):
    def __init__(self, parameter):
        self.parameter = parameter

    def visit_Function(self, term):
        term.arguments.append(clingo.ast.Symbol(term.location, self.parameter))
        return term

    def visit_Symbol(self, term):
        # this function is not necessary if gringo's parser is used
        # but this case could occur in a valid AST
        fun = term.symbol
        assert(fun.type == clingo.SymbolType.Function)
        term.symbol = clingo.Function(fun.name, fun.arguments + [self.parameter], fun.positive)
        return term

class ProgramTransformer(Transformer):
    def __init__(self, parameter):
        self.parameter = parameter
        self.term_transformer = TermTransformer(parameter)

    def visit_SymbolicAtom(self, atom):
        atom.term = self.term_transformer.visit(atom.term)
        return atom

    def visit_Program(self, prg):
        prg.parameters.append(clingo.ast.Id(prg.location, self.parameter.name))
        return prg

    def visit_ShowSignature(self, sig):
        sig.arity += 1
        return sig

    def visit_ProjectSignature(self, sig):
        sig.arity += 1
        return sig

def main(prg):
    with prg.builder() as b:
        t = ProgramTransformer(clingo.Function("k"))
        clingo.parse_program(
            open("example.lp").read(),
            lambda stm: b.add(t.visit(stm)))
    prg.ground([("base", [clingo.Number(i)]) for i in range(3)])
    prg.solve()

#end.
