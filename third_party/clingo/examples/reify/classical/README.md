# Computing Classical Models

Using meta-programming for computing classical models of logic programs.


## Example Call

Finding all classical models of `example1.lp`:

    $ clingo --output=reify example1.lp | clingo -Wno-atom-undefined - encoding.lp 0

## Warning


The call returns classical models of the ground logic program that results
after grounding the input program (`example1.lp`). You can run the following
call to see that ground logic program:

    $ clingo --text example1.lp

The grounding process applies simplifications that may eliminate some atoms of
the input program. To avoid this, in `example1.lp` we have added an external
declaration. You can comment it and run the call again to see what happens.

