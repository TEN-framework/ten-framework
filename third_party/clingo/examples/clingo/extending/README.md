Exmaple to show different usages of the clingo module.

Examples
========

Embedding python code to implement a function evaluated during grounding:

    $ clingo embedded.lp example.lp 
    clingo version 5.5.0
    Reading from embedded.lp ...
    Solving...
    Answer: 1
    num(3) num(6) div(3,1) div(3,3) div(6,1) div(6,2) div(6,3) div(6,6)
    SATISFIABLE

    Models       : 1
    Calls        : 1
    Time         : 0.028s (Solving: 0.00s 1st Model: 0.00s Unsat: 0.00s)
    CPU Time     : 0.028s

Do something similar as above but use clingo's python module:

    $ python module.py
    num(3) num(6) div(3,1) div(3,3) div(6,1) div(6,2) div(6,3) div(6,6)

Writing a custom application:

    $ python app.py example.lp
    example version 1.0
    Reading from example.lp
    Solving...
    Answer: 1
    num(3) num(6) div(3,1) div(3,3) div(6,1) div(6,2) div(6,3) div(6,6)
    SATISFIABLE

    Models       : 1+
    Calls        : 1
    Time         : 0.003s (Solving: 0.00s 1st Model: 0.00s Unsat: 0.00s)
    CPU Time     : 0.001s
