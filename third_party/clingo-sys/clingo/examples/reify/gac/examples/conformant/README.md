# Conformant planning

Conformant planning can be represented in `QBF` style as:

* There exists an initial situation `I`, and a sequence of actions `S` that
  achieve the goal starting from `I`, such that for all initial situations
  `I'`, `S` achieves the goal starting from `I'`.

This is equivalent to:

* There exists an initial situation `I`, and a sequence of actions `S` that
  achieve the goal starting from `I`, such that there is no `I'` such that `S`
  does not achieve the goal starting from `I'`.

In our approach the guess program represents the first exists part:

* find an initial situation `I`, and a sequence of actions `S` that achieve the
  goal starting from `I`.

The check program represents the second part:

* find an initial situation `I'`, such that `S` (given by the guess program)
  does not achieve the goal starting from `I'`.

    $ ../../run.sh base.lp instance.lp guess.lp -- base.lp instance.lp check.lp
    clingo version 5.5.0
    Reading from - ...
    Solving...
    Answer: 1
    occurs(cpa_go_down(cpa_e0,cpa_f1,cpa_f0),1) occurs(cpa_step_in(cpa_e0,cpa_f0,cpa_p0),2)
    occurs(cpa_go_up(cpa_e0,cpa_f0,cpa_f1),3) occurs(cpa_step_out(cpa_e0,cpa_f1,cpa_p0),4)
    occurs(cpa_collect(cpa_c1,cpa_f1,cpa_p0),5) occurs(cpa_collect(cpa_c0,cpa_f1,cpa_p0),6)
    occurs(cpa_move_right(cpa_f1,cpa_p0,cpa_p1),7) occurs(cpa_collect(cpa_c1,cpa_f1,cpa_p1),8)
    occurs(cpa_collect(cpa_c0,cpa_f1,cpa_p1),9)
    SATISFIABLE

    Models       : 1+
    Calls        : 1
    Time         : 264.631s (Solving: 264.46s 1st Model: 264.46s Unsat: 0.00s)
    CPU Time     : 264.527s
