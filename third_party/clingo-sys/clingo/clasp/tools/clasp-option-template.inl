#define R(lo,hi) [lo,hi]
#define L(...) {__VA_ARGS__}
#if defined(WITH_RANGES) && WITH_RANGES == 1
#define VALS(X,Y) X
#define T(X) X
#else
#define VALS(X,Y) Y
#define T(X)
#endif
#ifndef CLASP_BINARY
#define CLASP_BINARY clasp3
#endif
#define $Configuration # Configuration:

// -----
// clasp-3.3.x option parameter file
$Configuration -DWITH_RANGES=WITH_RANGES -DWITH_ASP=WITH_ASP -DWITH_OPTIMIZATION=WITH_OPTIMIZATION
// -----
// F: flag (yes = flag set; no = flag removed)
// S: skip (only to model constraints for the parameters; not passed to clasp)
// :[String]: will not be parsed for clasp (only syntatic sugar for the parameter file)
// :[int]: alignment of complex parameters
// i : integer range
// l : logarithmic transformation (n > 0)
// -----
// --- PREPROCESSING - global options
@0:solver {CLASP_BINARY}[CLASP_BINARY]
@0:configuration {auto,frumpy,jumpy,tweety,handy,crafty,trendy}[auto] # default config for options not explicitly given
@0:F:learn-explicit{yes,no}[no]
@0:S:sat-prepro {yes,no}[yes]
@0:sat-prepro {0}[0]
@0:1:sat-prepro {1,2,3}[2]                             # algo type
@0:2:sat-prepro VALS(R(1,50), L(1,10,20,25,50))[10]T(il) # iterations  
@0:3:sat-prepro VALS(R(1,50), L(1,10,20,25,50))[25]T(il) # cutoff    
@0:4:sat-prepro {0}[0]                                 # timelimit -> non deterministic 
@0:5:sat-prepro VALS(R(0,100), L(0,50,100))[0]T(i)       # max percent frozen
#if defined(WITH_ASP) && WITH_ASP
// ASP OPTIONS
@0:S:eqCond {yes,no}[no]       
@0:no:eq {0}[0]                
@0:eq VALS(R(1,127), L(1,2,3,5,10,-1))[5] T(il)
@0:F:backprop{yes,no}[yes]
@0:F:eq-dfs{yes,no}[no]        
@0:F:no-gamma{yes,no}[no]      
@0:trans-ext {all,choice,card,weight,integ,dynamic,no}[dynamic]
@1:partial-check {0,10,20,30,40,50}[0] # only for disjunctive ASP
@1:loops {common,distinct,shared,no}[no]
@1:score-other {no,loop,all}[loop]
#endif
// @1:enum-mode {bt, record, auto}[auto]
#if defined(WITH_OPTIMIZATION)
// OPTIMIZATION OPTIONS
@1:0:opt-strategy      {bb,usc}          [bb]
@1:1:BB:opt-strategy   {lin,hier,inc,dec}[lin]
@1:1:USC:opt-strategy  {oll,one,pmres,k} [oll]
@1:2:USC:opt-strategy  VALS(R(0,7)   , L(0,1,2,3,4,5,6,7))[1]T(i)
@1:2:USCK:opt-strategy VALS(R(0,1024), L(0,2,4,8,16,32,48,64,128,256,512,1024))[0]T(i)
@1:3:USCK:opt-strategy VALS(R(0,7)   , L(0,1,2,3,4,5,6,7))[1]T(i)
@1:0:opt-usc-shrink   {lin,inv,bin,rgs,exp,min,no}[no]
@1:1:opt-usc-shrink   VALS(R(0,12), L(2,4,8,10,12,14))[10]T(i)
@1:opt-heuristic {0,1,2,3}[0] 
@1:F:restart-on-model {yes,no}[no]
@1:reset-restarts {no,repeat,disable}[no]
#endif
// --- HEURISTICS
@1:0:heuristic {Berkmin,Vmtf,Vsids,Unit,Domain,None}[Vsids]
@1:1:vsids:heuristic {75,85,92,93,94,95,96,97,98,99} [95]    # Conditional - heuristic=vsids
@1:1:vmtf:heuristic VALS(R(1,128), L(0,4,8,16,32,64,128))[8]T(il) # Conditional - heuristic=vmtf
@1:1:berk:heuristic {0,128,256,512,1024,2048}[0]             # Conditional - heuristic=berkmin
@1:S:vsids-progress {yes,no}[no]
@1:No:vsids-progress {no}[no]
@1:1:vsids-progress VALS(R(80,95), L(80,82,84,85,86,87,88,90,91,92,93,94,95))[80]T(i) # Conditional - heuristic=vsids
@1:2:vsids-progress VALS(R(1,15) , L(1,2,3,4,5,6,7,8,9,10))[1]T(i)
@1:3:vsids-progress VALS(R(100,30000), L(100,500,1000,2000,5000,10000,15000,30000))[5000]T(il)
@1:F:init-moms {yes,no}[no]
@1:F:nant {yes,no}[no]
@1:sign-def {asp,pos,neg,rnd}[asp]
@1:F:sign-fix {yes,no}[no]
@1:score-res {min,set,multiset}[set] # Conditional - only for lookback heuristics
@1:F:berk-huang {yes,no}[no]         # Conditional - heuristic=berkmin
@1:F:vsids-acids {yes,no}[no]        # Conditional - heuristic=vsids|doman
@1:0:dom-mod {level,pos,true,neg,false,init,factor}[level] # Conditional - heuristic=Domain
@1:1:dom-mod {0,1,8,24}[0]           # Conditional - heuristic=Domain  
@1:save-progress VALS(R(0,180), L(0,1,10,50,100,180,250))[180]T(i)
@1:init-watches {rnd,first,least}[least]
@1:1:lookahead {atom,body,hybrid,no}[no]  
@1:2:lookahead VALS(R(1,2147483647), L(1,10,50,100,2147483647))[1]T(il)   # Conditional - 1:lookahead != no
@1:rand-freq {0.0,0.01,0.02,0.05,0.1}[0.0]
// --- RESTARTS
@1:0:restarts {F,L,D,x,+,no}[x]
// first parameter for all "normal" restarts
@1:1:Simp:restarts VALS(R(1,65535), L(1,10,100,128,200,256,300,500,1000,1600,2000,5000,10000,16000,65535))[128]T(il) 
// Luby restarts
@1:S:Luby:aryrestarts {1,2}[1]
@1:2:Luby:restarts VALS(R(1,65535), L(1,10,50,100,1000,16000,65535))[1000]T(il)
// Geometric restarts
@1:S:Geo:aryrestarts {2,3}[2]
@1:2:Geo:restarts VALS(R(1.0,2.0), L(1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0))[1.5]
@1:3:Geo:restarts VALS(R(1,65535), L(1,10,15,20,50,100,1000,16000,65535))[1]T(il)
// Arithmethic restarts
@1:S:Ari:aryrestarts {2,3}[2]
@1:2:Ari:restarts VALS(R(1,65535), L(1,10,50,100,200,500,1000,1500,5000,10000,16000,65535))[100]T(il)
@1:3:Ari:restarts VALS(R(1,65535), L(1,10,15,20,50,100,1000,16000,65535))[1]T(il)
// Dynamic restarts
@1:S:Dyn:aryrestarts {2,3}[2]
@1:1:Dyn:restarts VALS(R(50,1000), L(50,60,80,100,150,250,500,1000))[100]T(il)
@1:2:Dyn:restarts VALS(R(0.5,1.0), L(0.5,0.6,0.66,0.7,0.75,0.8,0.9,1.0))[0.7]
@1:3:Dyn:restarts VALS(R(20,127), L(20,25,32,40,50,60,100,127))[32]T(il)
// Other restart options
@1:F:local-restarts {yes,no}[no]
@1:S:counterCond {yes,no} [yes]
@1:1:counter-restarts VALS(R(1,127), L(1,3,5,7,9,11,13,19,27,31,39,43,57,67,93,127))[3]T(il)
@1:2:counter-restarts VALS(R(10,16384), L(10,16,32,64,128,512,1023,2013,4032,9973,13193))[10]T(il)
@1:0:block-restarts {0,1000,2000,5000,7500,10000}[0]
@1:1:block-restarts {1.2,1.4,1.6,1.8,2.0}[1.4]
@1:2:block-restarts {0,5000,10000}[0]
// --- DELETION
@1:S:deletion {yes,no}[yes]
@1:deletion {no}[no]
@1:1:deletion {basic,sort,ipSort,ipHeap}[basic] # Algorithm
@1:2:deletion VALS(R(10,100), L(10,20,30,40,50,60,66,70,75,80,90))[75]T(i)  # Fraction to remove
@1:3:deletion {activity,lbd,mixed}[activity]                                # Score function
@1:1:del-init VALS(R(1.0,50.0) , L(1.0,2.0,3.0,5.0,10.0,20.0,30.0,50.0))[3.0]
@1:2:del-init VALS(R(10,1023)  , L(10,50,100,500,1023,2000))[1023]T(il)
@1:3:del-init VALS(R(500,32767), L(500,1000,2500,4600,5800,9000,15000,20000,32767))[9000]T(i)
@1:del-max VALS(R(32767,2147483647), L(32767,50000,100000,250000,500000,1000000,2147483647))[250000]T(i)
@1:del-estimate {0,1,2,3}[0]                    # only for ASP + PB
@1:del-on-restart VALS(R(0,50), L(0,10,20,33,50,66))[0]T(i)
@1:1:del-glue VALS(R(0,8), L(0,2,3,4,5,6,7,8))[2]T(i)
@1:2:del-glue {0,1}[0]
@1:0:del-cfl {F,L,x,+,no}[no]
// Deletion conflict schedule
@1:1:del-cfl   VALS(R(1,65535), L(1,10,100,128,200,256,300,500,1000,1600,2000,5000,10000,16000,65535))[128]T(il)
@1:2:G:del-cfl VALS(R(1.0,2.0), L(1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0))[1.5]
@1:2:A:del-cfl VALS(R(1,65535), L(1,10,50,100,200,500,1000,1500,5000,10000,16000,65535))[100]T(il)
@1:3:del-cfl   VALS(R(10,65535), L(1,10,15,20,50,100,1000,16000,65535))[10]T(il)
@1:S:del-grow {yes,no}[yes]           # Conditional: enable/disable grow strategy
@1:del-grow {0}[0]
@1:1:del-grow VALS(R(1.0,5.0)  , L(1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0,3.0,4.0,5.0))[1.1]
@1:2:del-grow VALS(R(0.0,100.0), L(0.0,3.0,10.0,20.0,25.0,33.0,50.0,66.0,75.0,100.0))[20.0]
@1:S:growSched {yes,no}[no]
// Deletion size schedule
@1:3:del-grow {F,L,x,+}[+]
@1:4:del-grow   VALS(R(1,65535), L(1,10,100,128,200,256,300,500,1000,1600,2000,5000,10000,16000,65535))[128]T(il)
@1:5:G:del-grow VALS(R(1.0,2.0), L(1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0))[1.5]
@1:5:A:del-grow VALS(R(1,65535), L(1,10,50,100,200,500,1000,1500,5000,10000,16000,65535))[100]T(il)
@1:6:del-grow VALS(R(10,65535), L(1,10,15,20,50,100,1000,16000,65535))[10]T(il)
// --- MISC    
@1:0:strengthen {local,recursive,no}[local] # Conditional
@1:1:strengthen {all,short,binary}[all]
@1:2:strengthen {yes,no}[yes]
@1:otfs {0,1,2}[2]
@1:1:update-lbd {no,less,glucose,pseudo}[no]
@1:2:update-lbd VALS(R(0,30), L(0,2,3,4,8,16,24,30))[0]T(i)
@1:F:update-act {yes,no}[no]      
@1:reverse-arcs {0,1,2,3}[1]
@1:S:contraction {yes,no}[no]
@1:No:contraction {no}[no]
@1:contraction VALS(R(1,1023), L(0,80,100,150,166,200,250,500,1000,2000,5000))[250]T(il)           # Conditional
// --- CONDITIONS/CONSTRAINTS
// sat-prepro
@0:sat-prepro   | @0:S:sat-prepro in {no}
@0:1:sat-prepro | @0:S:sat-prepro in {yes}
@0:2:sat-prepro | @0:S:sat-prepro in {yes}
@0:3:sat-prepro | @0:S:sat-prepro in {yes}
@0:4:sat-prepro | @0:S:sat-prepro in {yes}
@0:5:sat-prepro | @0:S:sat-prepro in {yes}
#if defined(WITH_ASP) && WITH_ASP
@0:no:eq | @0:S:eqCond in {no}
@0:eq | @0:S:eqCond in {yes}
@0:F:eq-dfs | @0:S:eqCond in {yes}
#endif
// heuristic conds
@1:F:berk-huang | @1:0:heuristic in {Berkmin}
@1:F:vsids-acids | @1:0:heuristic in {Vsids,Domain}
@1:score-res | @1:0:heuristic in {Berkmin,Vmtf,Vsids,Domain}
@1:0:dom-mod | @1:0:heuristic in {Domain}
@1:1:dom-mod | @1:0:heuristic in {Domain}
@1:1:vsids:heuristic | @1:0:heuristic in {Vsids,Domain}
@1:1:vmtf:heuristic  | @1:0:heuristic in {Vmtf}
@1:1:berk:heuristic  | @1:0:heuristic in {Berkmin}
@1:S:vsids-progress  | @1:0:heuristic in {Vsids,Domain}
@1:No:vsids-progress | @1:S:vsids-progress in {no}
@1:1:vsids-progress  | @1:S:vsids-progress in {yes}
@1:2:vsids-progress  | @1:S:vsids-progress in {yes}
@1:3:vsids-progress  | @1:S:vsids-progress in {yes}
@1:2:lookahead | @1:1:lookahead in {atom,body,hybrid}
// opt conds
@1:1:BB:opt-strategy  | @1:0:opt-strategy in {bb}
@1:1:USC:opt-strategy | @1:0:opt-strategy in {usc}
@1:2:USC:opt-strategy | @1:1:USC:opt-strategy in {oll,one,pmres}
@1:2:USCK:opt-strategy| @1:1:USC:opt-strategy in {k}
@1:3:USCK:opt-strategy| @1:1:USC:opt-strategy in {k}
@1:0:opt-usc-shrink   | @1:0:opt-strategy in {usc}
@1:1:opt-usc-shrink   | @1:0:opt-strategy in {usc}
@1:1:opt-usc-shrink   | @1:0:opt-usc-shrink in {lin,inv,bin,rgs,exp,min}
// lookback conds
// ... |@1:F:no-lookback in {no}
// restarts conds
@1:1:Simp:restarts | @1:0:restarts in {F,L,x,+} 
@1:S:Luby:aryrestarts | @1:0:restarts in {L}
@1:2:Luby:restarts | @1:0:restarts in {L}
@1:2:Luby:restarts | @1:S:Luby:aryrestarts in {2}
@1:S:Geo:aryrestarts | @1:0:restarts in {x}
@1:2:Geo:restarts | @1:0:restarts in {x}
@1:3:Geo:restarts | @1:0:restarts in {x}
@1:3:Geo:restarts | @1:S:Geo:aryrestarts in {3}
@1:S:Ari:aryrestarts | @1:0:restarts in {+}
@1:2:Ari:restarts | @1:0:restarts in {+}
@1:3:Ari:restarts | @1:0:restarts in {+}
@1:3:Ari:restarts | @1:S:Ari:aryrestarts in {3}
// dynamic restarts
@1:1:Dyn:restarts    | @1:0:restarts in {D}
@1:S:Dyn:aryrestarts | @1:0:restarts in {D}
@1:2:Dyn:restarts    | @1:0:restarts in {D}
@1:3:Dyn:restarts    | @1:0:restarts in {D}
@1:1:counter-restarts | @1:S:counterCond in {yes}
@1:1:counter-restarts | @1:0:restarts in {F,D,L,x,+}
@1:2:counter-restarts | @1:S:counterCond in {yes}
@1:2:counter-restarts | @1:0:restarts in {F,D,L,x,+}
@1:1:block-restarts | @1:0:block-restarts in {1000,2000,5000,7500,10000}
@1:2:block-restarts | @1:0:block-restarts in {1000,2000,5000,7500,10000} 
// deletion
@1:deletion       | @1:S:deletion in {no}
@1:1:deletion     | @1:S:deletion in {yes}
@1:2:deletion     | @1:S:deletion in {yes}
@1:3:deletion     | @1:S:deletion in {yes}
@1:1:del-init     | @1:S:deletion in {yes}
@1:2:del-init     | @1:S:deletion in {yes}
@1:3:del-init     | @1:S:deletion in {yes}
@1:del-max        | @1:S:deletion in {yes}
@1:del-estimate   | @1:S:deletion in {yes}
@1:del-on-restart | @1:S:deletion in {yes}
@1:1:del-glue     | @1:S:deletion in {yes}
@1:2:del-glue     | @1:S:deletion in {yes}
@1:0:del-cfl      | @1:S:deletion in {yes}
@1:1:del-cfl      | @1:0:del-cfl in {F,L,x,+}
@1:2:G:del-cfl    | @1:0:del-cfl in {x}
@1:2:A:del-cfl    | @1:0:del-cfl in {+}
@1:3:del-cfl      | @1:0:del-cfl in {x,+}
@1:del-grow       | @1:S:del-grow in {no}
@1:1:del-grow     | @1:S:del-grow in {yes}
@1:2:del-grow     | @1:S:del-grow in {yes}
@1:S:growSched    | @1:S:del-grow in {yes}
@1:3:del-grow     | @1:S:growSched in {yes}
@1:4:del-grow     | @1:S:growSched in {yes}
@1:5:G:del-grow   | @1:3:del-grow in {x}
@1:5:A:del-grow   | @1:3:del-grow in {+}
@1:6:del-grow     | @1:3:del-grow in {x,+}
{@1:S:deletion=yes, @1:0:del-cfl=no, @1:S:del-grow=no}
{@1:S:deletion=no, @1:S:del-grow=yes}
// misc
@1:1:strengthen | @1:0:strengthen in {local,recursive}
@1:2:strengthen | @1:0:strengthen in {local,recursive}
@1:contraction | @1:S:contraction in {yes}
@1:No:contraction | @1:S:contraction in {no}
@1:2:update-lbd   | @1:1:update-lbd in {less,glucose,pseudo}
// Forbidden:
// {@1:F:restart-on-model=yes, @1:enum-mode=bt}

