# Solving 2QBF

Given the following 2QBF:

    EXISTS {x(1),x(2)} FORALL {y(1),y(2)} (-x(1) OR -y(1)) AND (-x(2) v -y(2))

the next call returns the unique solution, where both `x(1)` and `x(2)` are false:

    $ ../../run.sh base.lp guess.lp -- check.lp -- 0
    Reading from - ...
    Solving...
    Answer: 1

    SATISFIABLE

    Models       : 1
    Calls        : 1
    Time         : 0.008s (Solving: 0.00s 1st Model: 0.00s Unsat: 0.00s)
    CPU Time     : 0.008s
