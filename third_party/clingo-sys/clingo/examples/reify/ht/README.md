# Here-and-There models

Computing Here-and-There models of logic programs


## Example Calls

Finding all Here-and-There models of `example1.lp`:

    $ clingo --output=reify example1.lp | clingo -Wno-atom-undefined - encoding.lp 0 -c option=1

Finding all Here-and-There models minimizing the Here world of `example1.lp`:

    $ clingo --output=reify example1.lp | clingo -Wno-atom-undefined - encoding.lp 0 -c option=2

Finding all Equilibrium models of `example1.lp`:

    $ clingo --output=reify example1.lp | clingo -Wno-atom-undefined - encoding.lp 0 -c option=3
