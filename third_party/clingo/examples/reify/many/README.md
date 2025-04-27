# Many diverse stable models

Computing many diverse stable models of logic programs


## Example Calls

Finding 3 different (or 1-diverse) stable models of `example.lp`:

    $ clingo --output=reify example.lp | clingo -Wno-atom-undefined - encoding.lp -c m=3 -c option=1 -c k=1

Finding 3 6-diverse stable models of `example.lp`:

    $ clingo --output=reify example.lp | clingo -Wno-atom-undefined - encoding.lp -c m=3 -c option=1 -c k=6

Finding 3 most diverse stable models of `example.lp`:

    $ clingo --output=reify example.lp | clingo -Wno-atom-undefined - encoding.lp -c m=3 -c option=2 --quiet=1,2,2

