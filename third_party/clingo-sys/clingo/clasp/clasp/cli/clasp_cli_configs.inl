//
// Copyright (c) 2013-2017 Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
/*!
 * \file
 * \brief Supermacros for defining clasp's default configurations.
 * \code
 * CONFIG(sId, name, "<common>", "<stand-alone>", "<portfolio>")
 * \endcode
 *
 * A configuration consists of a solver id and a name followed by three option strings:
 * - "<common>"     : options that are always part of the configuration
 * - "<stand-alone>": options to add if configuration is used as a stand-alone configuration (e.g. global options)
 * - "<portfolio>"  : options to add if configuration is used in a portfolio
 * .
 * \note The solver id is used to identify a configuration in the default portfolio.
 */

#if !defined(CONFIG) || (!defined(CLASP_CLI_DEFAULT_CONFIGS) && !defined(CLASP_CLI_AUX_CONFIGS))
#error Invalid include context
#endif

//! Named default configurations accessible via option "--configuration=<name>".
#if defined(CLASP_CLI_DEFAULT_CONFIGS)
CLASP_CLI_DEFAULT_CONFIGS
CONFIG(0, tweety\
       , "--heuristic=Vsids,92 --restarts=L,60 --deletion=basic,50 --del-max=2000000 --del-estimate=1 --del-cfl=+,2000,100,20 --del-grow=0 --del-glue=2,0"\
         " --strengthen=recursive,all --otfs=2 --init-moms --score-other=all --update-lbd=less --save-progress=160 --init-watches=least --local-restarts --loops=shared"\
       , "--eq=3 --trans-ext=dynamic"\
       , "--opt-strat=bb,hier")
CONFIG(1, trendy\
       , "--heuristic=Vsids --restarts=D,100,0.7 --deletion=basic,50 --del-init=3.0,500,19500 --del-grow=1.1,20.0,x,100,1.5 --del-cfl=+,10000,2000 --del-glue=2"\
         " --strengthen=recursive --update-lbd=less --otfs=2 --save-p=75 --counter-restarts=3,1023 --reverse-arcs=2 --contraction=250 --loops=common"\
       , "--sat-p=2,iter=20,occ=25,time=240 --trans-ext=dynamic"\
       , "--opt-heu=sign --opt-strat=usc,disjoint")
CONFIG(2, frumpy\
       , "--heuristic=Berkmin --restarts=x,100,1.5 --deletion=basic,75 --del-init=3.0,200,40000 --del-max=400000 --contraction=250 --loops=common --save-p=180"\
         " --del-grow=1.1 --strengthen=local --sign-def-disj=pos"\
       , "--eq=5"\
       , "--restart-on-model --opt-heu=model")
CONFIG(3, crafty\
       , "--restarts=x,128,1.5 --deletion=basic,75 --del-init=10.0,1000,9000 --del-grow=1.1,20.0 --del-cfl=+,10000,1000 --del-glue=2 --otfs=2"\
         " --reverse-arcs=1 --counter-restarts=3,9973 --contraction=250"\
       , "--sat-p=2,iter=10,occ=25,time=240 --trans-ext=dynamic --backprop --heuristic=Vsids --save-p=180"\
       , "--heuristic=domain --dom-mod=neg,opt --opt-strat=bb,hier")
CONFIG(4, jumpy\
       , "--heuristic=Vsids --restarts=L,100 --deletion=basic,75,mixed --del-init=3.0,1000,20000 --del-grow=1.1,25,x,100,1.5 --del-cfl=x,10000,1.1 --del-glue=2"\
         " --update-lbd=glucose --strengthen=recursive --otfs=2 --save-p=70"\
       , "--sat-p=2,iter=20,occ=25,time=240 --trans-ext=dynamic"\
       , "--restart-on-model --opt-heu=sign,model --opt-strat=bb,inc")
CONFIG(5, handy\
       , "--heuristic=Vsids --restarts=D,100,0.7 --deletion=sort,50,mixed --del-max=200000 --del-init=20.0,1000,14000 --del-cfl=+,4000,600 --del-glue=2 --update-lbd=less"\
         " --strengthen=recursive --otfs=2 --save-p=20 --contraction=600 --loops=distinct --counter-restarts=7,1023 --reverse-arcs=2"\
       , "--sat-p=2,iter=10,occ=25,time=240 --trans-ext=dynamic --backprop"\
       , "")
#undef CLASP_CLI_DEFAULT_CONFIGS
#endif
//! Auxiliary configurations accessible via default portfolio ("--configuration=many").
#if defined(CLASP_CLI_AUX_CONFIGS)
CLASP_CLI_AUX_CONFIGS
CONFIG(6,  s6,  "--heuristic=Berkmin,512 --restarts=x,100,1.5 --deletion=basic,75 --del-init=3.0,200,40000 --del-max=400000 --contraction=250 --loops=common --del-grow=1.1,25 --otfs=2 --reverse-arcs=2 --strengthen=recursive --init-w=least --lookahead=atom,10", "", "")
CONFIG(7,  s7,  "--heuristic=Vsids --reverse-arcs=1 --otfs=1 --local-restarts --save-progress=0 --contraction=250 --counter-restart=7,200 --restarts=x,100,1.5 --del-init=3.0,800,-1 --deletion=basic,60 --strengthen=local --del-grow=1.0,1.0 --del-glue=4 --del-cfl=+,4000,300,100",  "", "")
CONFIG(8,  s8,  "--heuristic=Vsids --restarts=L,256 --counter-restart=3,9973 --strengthen=recursive --update-lbd=less --del-glue=2 --otfs=2 --deletion=ipSort,75,mixed --del-init=20.0,1000,19000", "", "")
CONFIG(9,  s9,  "--heuristic=Berkmin,512 --restarts=F,16000 --lookahead=atom,50",  "", "")
CONFIG(10, s10, "--heuristic=Vmtf --strengthen=no --contr=0 --restarts=x,100,1.3 --del-init=3.0,800,9200",  "", "")
CONFIG(11, s11, "--heuristic=Vsids --strengthen=recursive --restarts=x,100,1.5,15 --contraction=0",  "", "")
CONFIG(12, s12, "--heuristic=Vsids --restarts=L,128 --save-p --otfs=1 --init-w=least --contr=0 --opt-heu=sign,model",  "", "")
CONFIG(13, s13, "--heuristic=Berkmin,512 --restarts=x,100,1.5,6 --local-restarts --init-w=least --contr=0",  "", "")
CONFIG(14, nolearn, "--no-lookback --heuristic=Unit --lookahead=atom --deletion=no --restarts=no",  "", "")
CONFIG(15, tester,  "--heuristic=Vsids --restarts=D,100,0.7 --deletion=sort,50,mixed --del-max=200000 --del-init=20.0,1000,14000 --del-cfl=+,4000,600 --del-glue=2 --update-lbd=less"\
                    " --strengthen=recursive --otfs=2 --save-p=20 --contraction=600 --counter-restarts=7,1023 --reverse-arcs=2"\
                 ,  "--sat-p=2,iter=10,occ=25,time=240", "")
#undef CLASP_CLI_AUX_CONFIGS
#endif
#undef CONFIG
