'''
Example showing basic usage of clingo's application class.
'''

import sys
from typing import Sequence, Iterator

from clingo.symbol import Number, Symbol
from clingo.control import Control
from clingo.application import clingo_main, Application


class ExampleApp(Application):
    '''
    Example application class.
    '''
    program_name: str = "example"
    version: str = "1.0"

    @staticmethod
    def divisors(sym: Symbol) -> Iterator[Symbol]:
        '''
        Return all divisors of the given number.
        '''
        num = sym.number
        for i in range(1, num + 1):
            if num % i == 0:
                yield Number(i)

    def main(self, ctl: Control, files: Sequence[str]):
        '''
        Main function of the application.
        '''
        for path in files:
            ctl.load(path)
        if not files:
            ctl.load("-")
        ctl.ground([("base", [])], context=self)
        ctl.solve()


if __name__ == "__main__":
    clingo_main(ExampleApp(), sys.argv[1:])
