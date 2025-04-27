"""
Module providing the clingo-like pyclingo application.
"""
from clingo import Application, clingo_main


class PyClingoApplication(Application):
    """
    This is an example app mimimicking clingo.
    """

    program_name = "pyclingo"

    def main(self, control, files):
        for file_ in files:
            control.load(file_)
        if not files:
            control.load("-")
        control.ground([("base", [])])
        control.solve()


if __name__ == "__main__":
    clingo_main(PyClingoApplication())
