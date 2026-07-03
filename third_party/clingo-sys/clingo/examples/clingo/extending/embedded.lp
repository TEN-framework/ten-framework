#script (python)

from typing import Iterator

from clingo.symbol import Symbol, Number


def divisors(a: Symbol) -> Iterator[Symbol]:
    '''
    Return all divisors of the given number.
    '''
    a = a.number
    for i in range(1, a+1):
        if a % i == 0:
          yield Number(i)

#end.
