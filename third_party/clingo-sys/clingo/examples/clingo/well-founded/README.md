# Compte the well-founded model of a program

This examples computes the well-founded model of a normal logic program. It
prints true and unknown atoms before solving. Note that it does not print false
atoms because there are in general infinitely many of them.

Disclaimer: Writing an efficient algorithm to compute the well-founded model is
actually quite tricky. This example is not very well tested - there might be
bugs.

## Dependencies

The example needs the clingox and networkx python packages in addition to the
clingo package. The packages can be installed using:

    python3 -m pip install --user --upgrade --extra-index-url https://test.pypi.org/simple/ clingo-cffi clingox
    python3 -m pip install --user --upgrade networkx

# Example Calls

    ➜ python well-founded.py example.lp
    level version 1.0
    Reading from example.lp
    Facts:
    r s
    Unknown:
    u v x y
    Solving...
    UNSATISFIABLE

    Models       : 0
    Calls        : 1
    Time         : 0.001s (Solving: 0.00s 1st Model: 0.00s Unsat: 0.00s)
    CPU Time     : 0.001s
