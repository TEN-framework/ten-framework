"""
Example showing basic clingo usage of the clingo module.
"""

from typing import Iterator

from clingo.control import Control
from clingo.symbol import Number, Symbol


class ExampleApp:
    """
    Example application class.
    """

    @staticmethod
    def divisors(sym: Symbol) -> Iterator[Symbol]:
        """
        Return all divisors of the given number.
        """
        num = sym.number
        for i in range(1, num + 1):
            if num % i == 0:
                yield Number(i)

    def run(self):
        """
        Runs the example.
        """
        ctl = Control()
        ctl.load("example.lp")
        ctl.ground([("base", [])], context=self)
        ctl.solve(on_model=print)


if __name__ == "__main__":
    ExampleApp().run()
