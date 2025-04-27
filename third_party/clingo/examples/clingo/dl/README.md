This example implements a scaled down version of clingo-dl supporting
constraints of the following form:

    &diff{u-v} <= d :- [rule body].

Examples
========

Enumerating all solutions of a flowshop problem.

    $ python app.py fsE.lp fsI.lp 0
    clingo-dl version 1.0
    Reading from fsE.lp ...
    Solving...
    Answer: 1
    dl((a,1),1) dl(bound,16) dl((a,2),7) dl((b,1),0) dl((b,2),1) dl((c,1),4) dl((c,2),11) permutation(b,a) permutation(a,c)
    Answer: 2
    dl((a,1),6) dl(bound,20) dl((a,2),16) dl((b,1),5) dl((b,2),10) dl((c,1),0) dl((c,2),5) permutation(b,a) permutation(c,b)
    Answer: 3
    dl((a,1),0) dl(bound,19) dl((a,2),3) dl((b,1),8) dl((b,2),13) dl((c,1),3) dl((c,2),8) permutation(c,b) permutation(a,c)
    Answer: 4
    dl((a,1),6) dl(bound,16) dl((a,2),12) dl((b,1),0) dl((b,2),1) dl((c,1),1) dl((c,2),7) permutation(b,c) permutation(c,a)
    Answer: 5
    dl((a,1),0) dl(bound,18) dl((a,2),3) dl((b,1),3) dl((b,2),7) dl((c,1),4) dl((c,2),13) permutation(b,c) permutation(a,b)
    Answer: 6
    dl((a,1),5) dl(bound,20) dl((a,2),10) dl((b,1),8) dl((b,2),14) dl((c,1),0) dl((c,2),5) permutation(c,a) permutation(a,b)
    SATISFIABLE
    
    Models       : 6
    Calls        : 1
    Time         : 0.006s (Solving: 0.00s 1st Model: 0.00s Unsat: 0.00s)
    CPU Time     : 0.006s

Finding the optimal solution of a flowshop problem.

    $ python app.py --minimize-variable bound fsE.lp fsI.lp
    clingo-dl version 1.0
    Reading from examples/fsE.lp ...
    Solving...
    Answer: 1
    dl((a,1),1) dl(bound,16) dl((a,2),7) dl((b,1),0) dl((b,2),1) dl((c,1),4) dl((c,2),11) permutation(b,a) permutation(a,c)
    Found new bound: 16
    Solving...
    Optimum found
    UNSATISFIABLE
    
    Models       : 1
    Calls        : 2
    Time         : 0.006s (Solving: 0.00s 1st Model: 0.00s Unsat: 0.00s)
    CPU Time     : 0.006s
