from textwrap import dedent

from setuptools import find_packages, setup

setup(
    version="5.5.0.post6",
    name="clingo-cffi-system",
    description="CFFI-based bindings to the clingo solver.",
    long_description=dedent(
        """\
        This package provides CFFI-based bindings to the clingo solver.

        Clingo is part of the [Potassco](https://potassco.org) project for *Answer Set Programming* (ASP).
        ASP offers a simple and powerful modeling language to describe combinatorial problems as *logic programs*.
        The *clingo* system then takes such a logic program and computes *answer sets* representing solutions to the given problem.
        To get an idea, check our [Getting Started](https://potassco.org/doc/start/) page and the [online version](https://potassco.org/clingo/run/) of clingo.

        Temporarily, the API documentation of this project can be found [here](https://www.cs.uni-potsdam.de/~kaminski/pyclingo-cffi/).
        """
    ),
    long_description_content_type="text/markdown",
    author="Roland Kaminski",
    author_email="kaminski@cs.uni-potsdam.de",
    license="MIT",
    url="https://github.com/potassco/clingo",
    setup_requires=["cffi>=1.0.0"],
    cffi_modules=["build.py:ffi"],
    install_requires=["cffi>=1.0.0"],
    packages=["clingo"],
    package_data={"clingo": ["py.typed"]},
    python_requires=">=3.6",
)
