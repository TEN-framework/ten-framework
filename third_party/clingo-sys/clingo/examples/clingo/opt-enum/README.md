# Example to enumerate all solutions in lexicographical order

This example enumerates all solutions of a logic program in lexicographical
order where costs are determined using the minimize constraint.

The program should be called with option `--opt-mode=optN` and can optionally
use `-q1` to suppress intermediate solutions. A maximum number of solutions can
be requested as usual.

Option `--restore` can be used to heuristically guide the solver to the next
solution once all solutions with an equal cost have been enumerated.

## Example Calls

Note that the purpose of the domain heuristic in the examples below is just to
show case that a lot of intermediate solutions might be enumerated.

```
➜ python opt.py example1.lp --opt-mode=optN 0 --heu=domain -q1 --stats
...
Enumerate
  Enumerated : 27
  Intermediate: 10

➜ python opt.py example1.lp --opt-mode=optN 15 --heu=domain -q1 --stats
...
Enumerate
  Enumerated : 15
  Intermediate: 5
...
