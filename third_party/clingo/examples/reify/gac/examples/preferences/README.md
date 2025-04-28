# Preferences 

Call to compute all superset maximal stable models of `guess.lp`:

    $ ../../run.sh base.lp guess.lp -- base.lp check_superset.lp -- 0
    clingo version 5.5.0
    Reading from - ...
    Solving...
    Answer: 1
    a(1) a(2)
    SATISFIABLE

    Models       : 1
    Calls        : 1
    Time         : 0.009s (Solving: 0.00s 1st Model: 0.00s Unsat: 0.00s)
    CPU Time     : 0.012s

Call to compute all subset minimal stable models of `guess.lp`:

    $ ../../run.sh base.lp guess.lp -- base.lp check_subset.lp -- 0
    clingo version 5.5.0
    Reading from - ...
    Solving...
    Answer: 1
    a(2)
    Answer: 2
    a(1)
    SATISFIABLE

    Models       : 2
    Calls        : 1
    Time         : 0.009s (Solving: 0.00s 1st Model: 0.00s Unsat: 0.00s)
    CPU Time     : 0.012s
