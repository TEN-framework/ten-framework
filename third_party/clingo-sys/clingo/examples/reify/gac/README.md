# Guess and Check Programming

An implementation of Guess and Check Programming [1] in clingo using
meta-programming (described in [2]).

A Guess and Check program is a pair of logic programs `<G,C>`. A set of atoms
`X` is a stable model of `<G,C>` if `X` is a stable model of `G`, and `C`
together with `H` is unsatisfiable, where `H` contains the facts `guess(x).`
for all atoms of the form `guess(x)` in `X`.

The implementation translates a Guess and Check program into a disjunctive
logic program, that is solved by clingo.


## Usage

The following call computes solutions for the given `[guess_programs]` and
`[check_programs]`:

    $ clingo --output=reify domain.lp [guess_programs]             | \
    grep "output(guess (.*))"                                      | \
    clingo --output=reify --reify-sccs - guess.lp [check_programs] | \
    clingo -Wno-atom-undefined - glue.lp [guess_programs]

Note that the `[guess_programs]` have to be given twice. The first time the
programs are passed is to compute the domain to ground the `[check_programs]`.

There is also a small script to simplify the above call:

    $ run.sh [guess_programs] -- [check_programs] -- [options]

See the readmes in examples subfolder for further informations.


## Notes

Predicate `guess/1` should not appear in any head of the `check_programs`, and
predicate `output/1` should not appear in the `check_programs`.


## References

[1] Thomas Eiter, Axel Polleres: Towards automated integration of guess and check programs in answer set programming: a meta-interpreter and applications. TPLP 6(1-2): 23-60 (2006).
[2] Roland Kaminski, Javier Romero, Torsten Schaub, Philipp Wanko: How to build your own ASP system?! Submitted for publication
