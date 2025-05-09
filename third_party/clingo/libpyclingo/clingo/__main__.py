"""
Module providing the clingo-like pyclingo application.
"""

from clingo.application import Application, ApplicationOptions, Flag, clingo_main
from clingo.script import enable_python


class PyClingoApplication(Application):
    """
    This is an example app mimimicking clingo.
    """

    program_name = "pyclingo"

    def __init__(self):
        self._enable_python = Flag()

    def register_options(self, options: ApplicationOptions):
        """
        Register additional options.
        """
        options.add_flag(
            "Basic Options",
            "enable-python",
            "Enable Python script tags",
            self._enable_python,
        )

    def main(self, control, files):
        if self._enable_python:
            enable_python()
        for file_ in files:
            control.load(file_)
        if not files:
            control.load("-")
        control.ground([("base", [])])
        control.solve()


if __name__ == "__main__":
    clingo_main(PyClingoApplication())
