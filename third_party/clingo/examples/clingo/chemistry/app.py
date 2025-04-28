'''
Example exploring program parts and externals.
'''

from typing import List, Sequence, Tuple, Optional

from clingo.control import Control
from clingo.symbol import Function, Number, Symbol

Parts = Sequence[Tuple[str, List[Symbol]]]
Externals = Sequence[Tuple[Symbol, Optional[bool]]]


def part_str(part: Tuple[str, List[Symbol]]):
    '''
    Return a nice string representation of the given program part to ground.
    '''
    if part[1]:
        return f'{part[0]}({", ".join(map(str, part[1]))})'
    return f'{part[0]}'


def run(files: Sequence[str], parts: Parts, assign: Externals = ()):
    '''
    Loads the given files into a control object, grounds the given program
    parts, and assign the given externals to the given truth values.
    '''
    ctl = Control()

    print('  loading files:')
    for file_ in files:
        print(f'  - {file_}')
        ctl.load(file_)

    print('  grounding:')
    for part in parts:
        print(f'  - {part_str(part)}')
        ctl.ground([part])

    if assign:
        print('  assigning externals:')
        for sym, truth in assign:
            print(f'  - {sym}={truth}')
            ctl.assign_external(sym, truth)

    print('  solutions:')
    ctl.solve(on_model=lambda m: print(f'  - {m}'))


print('Exmaple 1:')
run(['chemistry.lp'], [("base", [])])

print('\nExmaple 2:')
run(['chemistry.lp'], [("acid", [Number(42)])])

print('\nExmaple 3:')
run(['chemistry.lp', 'external.lp'],
    [("base", []), ("acid", [Number(42)])],
    [(Function("d", [Number(1), Number(42)]), True)])
