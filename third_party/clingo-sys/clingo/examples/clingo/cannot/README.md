# Using Propagators and Theory Language to Ease Debugging

To ease development of encodings, `&cannot` atoms can be used in rule heads to
get useful debugging messages.  The idea is to use them as rule heads where
otherwise integrity constraints would be used.  If an encoding produces unsat
results, this can be used to relax integrity constraints.  The script takes
care of minimizing the number of messages printed.  To get an idea, take a look
at `example.lp` and run one of the following calls:

    $ clingo cannot-py.lp example.lp
    $ clingo cannot-lua.lp example.lp
