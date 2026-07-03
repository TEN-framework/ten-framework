# Peg Solitaire

## The Problem

Given the board below, find a solution consisting of successive moves such that
only one peg (o) remains in the center of the board.  A move is an orthogonal
jump of length two above an adjacent peg to a free position on the board.  The
jumped peg has to be removed after the move.


        -------
        |o|o|o|
        -------
        |o|o|o|
    ---------------
    |o|o|o|o|o|o|o|
    ---------------
    |o|o|o| |o|o|o|
    ---------------
    |o|o|o|o|o|o|o|
    ---------------
        |o|o|o|
        -------
        |o|o|o|
        -------

## Setup

The visualizer requires urwid.  To install under Debian it, run as root:

    aptitude install python-urwid

## Finding a Solution

The problem can be solved using the incremental encoding in solitaire.lp.  To
find a solution, call (takes about 10s on a fast machine):

    clingo solitaire.lp instance.lp

To visualize the solution, call:

    ./visualize.py solitaire.lp instance.lp

## Notes

The encoding allows for parallel moves, i.e., moves that do not influence each
other may occur at the same time step.  This is essential to solve the problem.
When visualizing the solution the moves are sequentialized again.
