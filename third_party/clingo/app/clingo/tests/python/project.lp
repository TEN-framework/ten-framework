% author : T. Guyet
% date   : 02/2014
#const th = 3.

symb(S) :- th { seq(P,S) }, S=1..nbs.

#show pattern/2.

#script(python)
import sys, os
from clingo import Number
if sys.version_info[0] >= 3:
    import io as StringIO
else:
    import StringIO

modelid = 0

patterns=list()

def readsequence(filename):
    seq = list()
    src = StringIO.StringIO("""1 5
2 2
3 5
4 2
5 5
6 2
7 1
8 2
9 4
10 2
11 1
12 6
13 1
14 2
15 5
16 3
17 3
18 1
19 3
20 2
""")
    if src is None:
        print ("invalid filename:" + str(filename))
        return None
    predpos=0
    for line in src:
        lineelem = line.rstrip('\n\r').lstrip(' ').split(' ')
        pos=lineelem[0]
        if( pos==predpos ) :
            print ("invalid sequence : simultaneous events are not allowed")
            return None
        predpos=pos
        seq.append( int(lineelem[1]) )
    src.close()
    return seq

def genASPsequence(seq, prg):
    i=1
    s="#const nbs = "+str(max(seq)) +"."
    prg.add("base",[], s )
    for n in seq:
        s='seq('+str(i)+","+str(n)+').'
        prg.add("base", [], s)
        i=i+1

def on_model(model):
    pattern={}

    for atom in model.symbols(atoms=True):
        if atom.name()=="pattern":
            args = atom.arguments
            pattern[ args[0] ]=args[1]

    found=False
    for i in patterns:
        if i==pattern:
            found=True
            break
    if not found:
        patterns.append( pattern )

def main(prg):
    theseq = readsequence("we_cmp.seq.minepi")
    genASPsequence(theseq, prg)
    prg.configuration.solve.project = "auto"
    prg.configuration.solve.models  = "0"
    d = prg.get_const("ml")
    if d is None:
        d=4
    else:
        d = d.number

    prg.ground([("base",[])])
    prg.solve()

    for k in range(2,d+1):
        prg.ground([("incr",[Number(k)])])
        prg.solve()
#end.

1{ pattern(1, P) : symb(P) } 1.

{ occ(P, 1, P) : seq(P, S) } :- pattern(1,S).

:- seq(P, S), not occ(P, 1, P), pattern(1,S).

#program incr(n).

1{ pattern(n, P) : symb(P) } 1.

0{ occ(I,n,Q) : seq(Q,S), pattern(n,S), Q>P } 1 :- occ(I, n-1, P).

:- occ(I, n-1, P), occ(I, n, Q), seq(P',S), pattern(n,S), P<P', P'<Q.
:- occ(I, n, P), occ(J, n, P'), P=P', I<J.
:- occ(I, 1, P), occ(J, 1, P'), occ(I, n, Q), occ(J, n, Q'), P<P', Q'<Q, I!=J.

idocc(I,n) :- occ(I,n,Q).
:- { idocc(I,n) } th-1.



