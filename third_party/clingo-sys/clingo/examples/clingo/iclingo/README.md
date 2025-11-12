# Incremental Grounding and Solving

With clingo-4 there is no iclingo version dedicated to incremental solving
anymore.  This example shows how the same functionality can be implemented
using clingo's API.

For convenience, this functionality has been build into clingo and can be used
without requiring additional files:

    #include <incmode>.

## Example calls

    clingo incmode-int.lp example.lp
    clingo incmode-lua.lp example.lp
    clingo incmode-py.lp example.lp
