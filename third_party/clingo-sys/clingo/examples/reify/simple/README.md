# Simple Meta Encoding

This example simply reproduces the answer sets of the reified program.


## Example Call

Finding all stable models of `example.lp`:

    $ clingo --output=reify example.lp | clingo -Wno-atom-undefined - ../common/meta.lp 0
