Example Calls
=============

The encodings here have been developed within the [metasp] project allowing for
handling complex optimization criteria, e.g., inclusion-based minimization or
Pareto efficiency.

Non-reified + Cardinality Minimization
--------------------------------------

    $ clingo --opt-mode=optN -q1 0 example1.lp

Reified + Cardinality Minimization
----------------------------------

    $ clingo --rewrite-minimize --output=reify --reify-sccs example1.lp |\
      clingo -Wno-atom-undefined - encoding.lp \
      <(echo "optimize(0,1,card).") 0

Reified + Subset Minimization
-----------------------------

    $ clingo --rewrite-minimize --output=reify --reify-sccs example1.lp |\
      clingo -Wno-atom-undefined - encoding.lp \
      <(echo "optimize(0,1,incl).") 0

Reified + Subset Minimization + Query to Solve the Conformant Planning Problem
------------------------------------------------------------------------------

    $ clingo example2.lp --output=reify |\
      clingo -Wno-atom-undefined - encoding.lp --project 0


Improving Performance
=====================

The above calls use clingo's reification backend. Better performance can be
obtained by first preprocessing the program and then reifying a potentially
much smaller program:

    $ clingo --pre --rewrite-minimize ... |\
      reify --sccs |\
      clingo -Wno-atom-undefined - encoding.lp ...


[metasp]: https://potassco.org/labs/metasp/
