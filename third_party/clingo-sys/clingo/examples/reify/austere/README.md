# Austere logic programs

Computing stable models using a meta-encoding where default negation only
occurs in the constraints, as in austere logic programs [1].


## Example Call

Finding all stable models of `example.lp`:

    $ clingo --output=reify example.lp | clingo -Wno-atom-undefined - encoding.lp 0


## References

[1] Jorge Fandinno, Seemran Mishra, Javier Romero, Torsten Schaub: Answer Set Programming Made Easy. Submitted for publication
