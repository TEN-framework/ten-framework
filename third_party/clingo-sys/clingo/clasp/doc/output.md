# Status output

By default, clasp prints computed answer sets as well as some basic status information.

```
clasp version 3.4.0
Reading from queens8_base.inst
Solving...
Answer: 1 (Time: 0.002s)
q(1,6) q(2,2) q(3,7) q(4,1) q(5,3) q(6,5) q(7,8) q(8,4)
SATISFIABLE

Models       : 1+
Calls        : 1
Time         : 0.001s (Solving: 0.00s 1st Model: 0.00s Unsat: 0.00s)
CPU Time     : 0.001s
Threads      : 4        (Winner: 0)
```

- The output starts with the clasp version followed by the name of the file (or `stdin`) from which input is read.
- The `Solving...` line indicates that reading/preprocessing has finished and the solving process is running.
- During solving, each computed answer set is preceded by an `Answer: <n> (Time: <elapsed>)`.
- Solving concludes with an overall status and summary, where the status is one of the following:
    - `UNSATISFIABLE`: problem is proven to have no solution.
    - `SATISFIABLE`: at least one solution to the problem has been found.
    - `OPTIMUM FOUND`: an optimal solution has been found (only for optimization problems).
    - `UNKNOWN`      : search was interrupted before a more definitive result was reached.

The summary contains:

- `Models:` number of models found. A `+` indicates more models may exist.
- `Calls:` number of solve invocations (only relevant for multi-shot solving).
- `Time:` total **wall-clock** time elapsed including reading and preprocessing.
    - `Solving:` time spent on search.
    - `1st Model:` time to find the first model.
    - `Unsat`: time from the last model to the end of the search.
- `CPU Time:` total **system** time used. This is also influenced by the number of solving threads running in
  parallel.

**Note:** Additional information may be included in the `Answer:` lines and summary, depending on the reasoning mode:

1. Optimization:
    - Each answer includes its associated _cost_ (`Optimization: <cost>...`).
    - Progress updates are printed in the form of `Progression: [<lower>:<upper>]` lines whenever a new lower bound is
      found.
    - The summary includes the lowest _cost_, whether the optimum was found, and the number of optimal models
      enumerated (if > 1).
2. Brave/Cautious Consequences:
    - Each answer lists the atoms in the current estimate, followed by `Consequences: [<lower>:<upper>]` giving lower
      and upper bounds for the number of atoms in the consequences.
    - The summary includes the number of consequences computed and whether all brave/cautious consequences were found.
3. Parallel Search (option `--parallel-mode`)
    - The summary additionally includes `Threads:`, the number of threads used, and `Winner:`, the thread that
      terminated the search.

Additional status, progress, and summary output can be enabled via options `--stats` and `--verbose`.

## Statistics

When called with option `--stats`, `clasp` prints additional solving and problem statistics.
For parallel search, `--stats=2` additionally enables per thread solving statistics. When solving disjunctive logic
programs, `--stats` accepts a second parameter controlling the output of solving statistics from the tester.

**NOTE:** Statistics are subject to change. They are mainly intended for debugging.

### Solving Statistics:

```
Choices      : 66072   
Conflicts    : 52827    (Analyzed: 52826)
Restarts     : 172      (Average: 307.13 Last: 239 Blocked: 0)
Model-Level  : 33.5
Problems     : 1        (Average Length: 0.00 Splits: 0)
Lemmas       : 53391    (Deleted: 33339)
  Binary     : 1488     (Ratio:   2.79%)
  Ternary    : 2007     (Ratio:   3.76%)
  Conflict   : 52826    (Average Length:   22.9 Ratio:  98.94%) 
  Loop       : 565      (Average Length:   10.7 Ratio:   1.06%) 
  Other      : 0        (Average Length:    0.0 Ratio:   0.00%) 
Backjumps    : 52826    (Average:  1.24 Max:  16 Sum:  65426)
  Executed   : 52826    (Average:  1.24 Max:  16 Sum:  65426 Ratio: 100.00%)
  Bounded    : 0        (Average:  0.00 Max:   0 Sum:      0 Ratio:   0.00%)
```

Solving statistics consist of:

- `Choices:` number of decisions. If `clasp`'s _domain heuristic_ is active, this number is followed by `Domain:`, the
  number of decisions influenced by domain modifications.
- `Conflicts:` number of conflicts of which `Analyzed:` were analyzed by the conflict resolution procedure.
  Remaining conflicts were either handled by (chronological) backtracking or terminated the search.
- `Restarts:` total number of restarts performed:
    - `Average:` average number of conflicts between restarts.
    - `Last:` number of conflicts from the last restart to the end of search.
    - `Blocked:` number of times a restart was blocked and therefore not executed (only applicable
      when option `--block-restarts` is used).
- `Model-Level:`: average number of decision literals in models (only present if at least one model was found).
- `Problems:` number of different _guiding paths_ processed. For single-threaded, single-shot solving this is always
  one. In splitting based search (option `--parallel-mode=<t>,split`), `Splits:` is the number of times solvers split
  their guiding paths to (dynamically) create work for other solvers.
- `Lemmas:` total number of learned constraints:
    - `Deleted:` total number of constraints that were removed by the deletion strategy.
    - `Binary:` and `Ternary:` give the number of learned constraints of size 2 and 3, respectively, along with their
      ratio to the total number of learned constraints.
    - `Conflict:`/`Loop:`/`Other:` is the number of constraints that were derived from conflict analysis,
      unfounded set checking, or other means (e.g. theory/enumeration nogoods or lemma-exchange between solvers),
      respectively, along with their average length and ratio to the total number.
- `Backjumps:` corresponds to the number of analyzed conflicts:
    - `Average:`/`Max:` is the average/maximal distance to the _asserting level_ of the learned conflict clause, while
      `Sum:` is the sum of all levels that were removed.
    - `Executed:` and `Bounded:` are only relevant if jumps were restricted by the algorithm (e.g. backtrack-based
      enumeration and parallel search). The latter gives details about the backjumps that were restricted by the
      algorithm, while the former describes the backjumps that were fully executed.

For parallel search, the _lemma statistics_ additionally contain:

- `Distributed:` number of constraints sent to other solvers, along with their ratio and average literal block distance.
- `Integrated:`  number of constraints integrated from other solvers. Additionally, the ratio, number of unit
  constraints, and average backjump length required for integrating received constraints is given.

When solving disjunctive logic programs, statistics additionally contain:

- `Stab. Tests:` number of stability tests further divided into `Full:` and `Partial:` for tests on total and partial
  assignments, respectively.

### Program Statistics

Program statistics provide information about the input logic program, problem translation, and preprocessing.

```
Rules        : 11649    (Original: 10317)
  Choice     : 742     
Atoms        : 5742     (Original: 5664 Auxiliary: 78)
Disjunctions : 6        (Original: 8)
Bodies       : 9539     (Original: 8287)
  Count      : 12       (Original: 28)
Equivalences : 7837     (Atom=Atom: 492 Body=Body: 782 Other: 6563)
Tight        : No       (SCCs: 11 Non-Hcfs: 1 Nodes: 986 Gammas: 0)
Variables    : 3520     (Eliminated:    0 Frozen: 1266)
Constraints  : 11559    (Binary:  74.3% Ternary:  21.5% Other:   4.2%)
```

- `Rules`: total number of rules in the simplified and original logic program. The number is further split into
  categories:
    - `Choice:` number of choice rules.
    - `Minimize:` number of minimize constraints.
    - `Heuristic:` number of `#heuristic` statements.
    - `Acyc:` number of `#edge` statements.
- `Atoms:` number of atoms in the simplified and original program. `Auxiliary:` gives the number of atoms that were
  introduced during (extended) rule translation.
- `Disjunctions:` number of disjunctions in the simplified (i.e. shifted) and original program (only for disjunctive
  logic programs).
- `Bodies:` number of distinct rule bodies in the simplified and original program. This number is further split into
  `Count` and `Sum` for the number of count and sum aggregates, respectively. In the example above, 16 of the initial 28
  count bodies were translated to normal bodies.
- `Equivalences:` statistics from the equivalence preprocessor. The first number is the total number of simplifications.
  `Atom=Atom:` and `Body=Body:` are the numbers of equivalent atoms and bodies found, respectively,
  while `Other:` counts the number of atoms/bodies sharing a solver variable.
  As an example, for the simple program `a :- not b. b :- not a. c :- a.`, we get
    - `Atom=Atom: 1` because atom `c` is equivalent to `a`,
    - `Body=Body: 1` because body `{a}` is equivalent to body `{not b}`, and
    - `Other: 5` because atoms `a`, `b`, `c`, and bodies `{a}`, `{not a}` share a solver variable with `{not b}`.
- `Tight:` statistics on the strongly connected components of the logic program. `Yes` if the program is tight.
  `N/A` if option `--supp-models` is active and therefore unfounded set checking is disabled. Otherwise:
    - `SCCs:` number of strongly connected components.
    - `Non-Hcfs:` number of strongly connected components with a head-cycle (only for disjunctive logic programs).
    - `Nodes:` number of nodes in the dependency graph that is used in unfounded set checking.
    - `Gammas:` number of rules added during component shifting (only for disjunctive logic programs).
        - **NOTE**: Due to a bookkeeping bug in versions < `3.4.0`, this was always reported as `0`.
- `Variables:` number of solver (internal) variables required to represent the problem:
    - **NOTE**: `Eliminated:`/`Frozen:` are only relevant when option `--sat-prepro` is used.
    - `Eliminated:` number of variables eliminated by the preprocessor's _variable elimination_ procedure.
    - `Frozen:` number of variables that are excluded from elimination, e.g. because they appear in
      complex constraints.
- `Constraints:` number of boolean constraints after preprocessing. `Binary:`/`Ternary:` are clauses of length
  two/three, while `Other:` counts longer clauses and more complex constraints (e.g. weight constraints).

## Progress

Type and amount of printed progress information is controlled via option `--verbose`.

**NOTE:** Verbosity levels and progress information are subject to change. They are mainly intended for debugging.

Currently, `clasp` supports six different verbosity levels. **Level 0** disables all status and progress information.
**Level 1**, the default, provides the basic status information described above. Starting with verbosity **level 2**,
`clasp` prints additional information about preprocessing:

```
clasp version 3.4.0
Reading from duthen-990602.37.steps.13.asp
Reading      : 0.005s
Preprocessing: 0.004s
Sat-Prepro   : 0.002s (ClRemoved: 5815 ClAdded: 786 LitsStr: 286)
Solving...
```

- `Reading:` total **wall-clock** time spent on reading the input.
- `Preprocessing:` total **wall-clock** time spent on preprocessing the input.
- `Sat-Prepro:` total **wall-clock** time spent on and result of Sat-Preprocessing (only with option `--sat-prepro`):
    - `ClRemoved:` number of clauses removed
    - `ClAdded:` number of clauses added during variable elimination
    - `LitsStr:` total number of literals removed from clauses during self-subsumption resolution
    - For large instances, the `Sat-Prepro:` line provides progress information before showing the final results:
        - `S`: Subsumption
        - `E`: Variable Elimination
        - `B`: Blocked Clause Elimination

For parallel search, verbosity levels 2 and above also print important events and inter-thread messages.

```
------------------------------------------------------------------------------------------|
ID:T       Info                     Info                      Info               Time     |
------------------------------------------------------------------------------------------|
 0:X| SYNC            sent                                                  |      0.036s |
 0:L| [Solving+0.000s]               attach                                 |      0.036s |
 3:L| [Solving+0.000s]               attach                                 |      0.036s |
 1:L| [Solving+0.000s]               attach                                 |      0.036s |
 2:L| [Solving+0.000s]               attach                                 |      0.036s |
 2:X| SYNC            received                                              |      0.037s |
 3:X| SYNC            received                                              |      0.038s |
 1:X| SYNC            received                                              |      0.038s |
 1:X| SYNC            completed                           in         0.002s |      0.038s |
...
 2:X| TERMINATE       sent                                                  |      3.276s |
...
```

For any progress information, `ID` refers to a solver (thread) and `T` to the type of event.
In this case, `X` is an inter-thread message and `L` a log event.
The `Time` colum contains the elapsed **wall-clock** time since the start of the solver.

Verbosity **level 3** prints important search events.

```
------------------------------------------------------------------------------------------|
ID:T       Vars           Constraints         State            Limits            Time     |
       #free/#fixed   #problem/#learnt  #conflicts/ratio #conflict/#learnt                |
------------------------------------------------------------------------------------------|
 0:R|   3263/257    |   11190/9747    |     37827/0.779 |    2677/2000000   |      5.589s |
 0:D|   2299/614    |    9385/7694    |     40504/0.785 |    3500/2000000   |      6.100s |
 0:R|   2906/614    |    9385/10894   |     43764/0.791 |     240/2000000   |      6.720s |
 0:R|   2906/614    |    9385/10964   |     43838/0.791 |     166/2000000   |      6.735s |
 0:R|   2906/614    |    9385/11016   |     43900/0.791 |     104/2000000   |      6.747s |
 0:D|   1082/614    |    9385/9319    |     44004/0.791 |    3600/2000000   |      6.767s |
 ...
 0:E|   1382/2138   |    5886/1531    |     52827/0.800 |    2517/2000000   |      8.537s |
```

- `Vars` gives the current number of unassigned variables (`#free`) and the number of variables assigned on the
  top-level (`#fixed`).
- `Constraints` shows the number of problem and learned constraints.
- `State` contains the current number of conflicts and the ratio between conflicts and decisions.
- `Limits` gives the number of conflicts until the next event and the maximal number of
  learned constraints allowed (deletion is forced once this number is reached).
- Event `T` is one of `D`=Lemma deletion, `G`=Database size limit update, `R`=Restart, `E`=Search exit.

Verbosity **levels 4** and **5** are only relevant when solving disjunctive logic programs.
On **level 4**, progress information is restricted to stability checks, while **level 5** combines the information from
levels 3 and 4.

```
------------------------------------------------------------------------------------------|
ID:T       Vars           Constraints         State            Limits            Time     |
       #free/#fixed   #problem/#learnt  #conflicts/ratio #conflict/#learnt                |
------------------------------------------------------------------------------------------|
 0:R|   2560/236    |   13456/0       |         0/0.000 |    2000/2000000   |      0.000s |
------------------------------------------------------------------------------------------|
 0:P| Y HCC: 0      |    3095/22      |         3/1.500 | Time:      0.000s |      0.000s |
 0:F| N HCC: 0      |    3095/24      |         2/0.051 | Time:      0.000s |      0.001s |
...
```

- Event `T` is either `P` for a partial check or `F` for a check on a total assignment.
- `HCC`: is the head-cycle component that is checked preceded by `Y` if the check succeeded, `N` if the check found an
  unfounded set, or `?` if the check is ongoing.
- `Constraints` and `State` have the same meaning as described above but are concerned with the problem induced by the
  given HCC.
- The following `Time:` shows the **wall-clock** time spent on the stability check.
