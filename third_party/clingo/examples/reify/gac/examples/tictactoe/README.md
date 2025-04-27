# Simple Tic-Tac-Toe

In this example there is a 3x3 Tic-Tac-Toe square. The guessing player has to
place her 3 tokens in a winning position such that afterwards the checking
player cannot place her tokens in a winning position:

    $ ../../run.sh base.lp guess.lp -- base.lp check.lp -- 0
    clingo version 5.5.0
    Reading from - ...
    Solving...
    Answer: 1
    m(1,1) m(2,2) m(3,3)
    Answer: 2
    m(1,3) m(2,2) m(3,1)
    SATISFIABLE

    Models       : 2
    Calls        : 1
    Time         : 0.013s (Solving: 0.00s 1st Model: 0.00s Unsat: 0.00s)
    CPU Time     : 0.012s

