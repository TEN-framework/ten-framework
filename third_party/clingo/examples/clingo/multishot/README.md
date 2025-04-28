Example show casing multi-shot solving. It implements branch-and-bound-based
optimization and incremental solving.

Examples
========

The branch-and-bound example:

    $ python opt.py tohE.lp tohI.lp tohB.lp -c n=20
    opt-example version 1.0
    Reading from tohE.lp ...
    Solving...
    Answer: 1
    move(3,c,2)  move(4,b,1)  move(4,c,3)  move(2,b,4)  move(4,b,5)  move(4,a,6)  move(3,b,7)
    move(3,c,8)  move(4,b,9)  move(4,a,10) move(3,b,11) move(4,b,12) move(1,c,13) move(4,c,14)
    move(3,a,15) move(4,a,16) move(2,c,17) move(4,b,18) move(3,c,19) move(4,c,20)
    Found new bound: 20
    Solving...
    ...
    Solving...
    Answer: 1
    move(3,c,2)  move(4,b,1)  move(4,c,3)  move(2,b,4)  move(4,a,5)  move(3,b,6)  move(4,b,7)
    move(1,c,8)  move(4,c,9)  move(3,a,10) move(4,a,11) move(2,c,12) move(4,b,13) move(3,c,14)
    move(4,c,15)
    Found new bound: 15
    Solving...
    Optimum found
    UNSATISFIABLE
    
    Models       : 5
    Calls        : 6
    Time         : 0.015s (Solving: 0.01s 1st Model: 0.01s Unsat: 0.00s)
    CPU Time     : 0.015s

The incremental solving example:

    $ python inc.py tohE.lp tohI.lp
    inc-example version 1.0
    Reading from tohE.lp ...
    Solving...
    ...
    Solving...
    Answer: 1
    move(4,b,1)  move(3,c,2)  move(4,c,3)  move(2,b,4)  move(4,a,5)  move(3,b,6)  move(4,b,7)
    move(1,c,8)  move(4,c,9)  move(3,a,10) move(4,a,11) move(2,c,12) move(4,b,13) move(3,c,14)
    move(4,c,15)
    SATISFIABLE
    
    Models       : 1+
    Calls        : 16
    Time         : 0.020s (Solving: 0.00s 1st Model: 0.00s Unsat: 0.00s)
    CPU Time     : 0.020s
