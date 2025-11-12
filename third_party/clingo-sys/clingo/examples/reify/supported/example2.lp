{ a }.
c :- b, a.
b :- c, a.

#external b. % necessary to avoid clingo's simplifications
