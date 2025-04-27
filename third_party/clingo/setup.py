import sys
import site
from textwrap import dedent
from skbuild import setup

if not site.ENABLE_USER_SITE and "--user" in sys.argv[1:]:
    site.ENABLE_USER_SITE = True

setup(
    version = '5.7.1',
    name = 'clingo',
    description = 'CFFI-based bindings to the clingo solver.',
    long_description = dedent('''\
        This package provides CFFI-based bindings to the clingo solver.

        Clingo is part of the [Potassco](https://potassco.org) project for *Answer Set Programming* (ASP).
        ASP offers a simple and powerful modeling language to describe combinatorial problems as *logic programs*.
        The *clingo* system then takes such a logic program and computes *answer sets* representing solutions to the given problem.
        To get an idea, check our [Getting Started](https://potassco.org/doc/start/) page and the [online version](https://potassco.org/clingo/run/) of clingo.

        Please check the the [API documentation](https://potassco.org/clingo/python-api/current/clingo/) on how to use this module.
        '''),
    long_description_content_type='text/markdown',
    author = 'Roland Kaminski',
    author_email = 'kaminski@cs.uni-potsdam.de',
    license = 'MIT',
    url = 'https://github.com/potassco/clingo',
    install_requires=[ 'cffi' ],
    cmake_args=[ '-DCLINGO_MANAGE_RPATH=OFF',
                 '-DCLINGO_BUILD_APPS=OFF',
                 '-DCLINGO_BUILD_WITH_PYTHON=pip',
                 '-DCLINGO_BUILD_WITH_LUA=OFF',
                 '-DPYCLINGO_INSTALL_DIR=libpyclingo' ],
    packages=[ 'clingo' ],
    package_data={ 'clingo': [ 'py.typed', 'import__clingo.lib', 'clingo.h', 'clingo.hh' ] },
    package_dir={ '': 'libpyclingo' },
    python_requires=">=3.6"
)
