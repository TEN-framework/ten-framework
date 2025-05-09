Step: 1
acyc_edge(0,1,(a,b)) assume((neg(b),)) begin_step end_step external(a,"False") heuristic(a,"Sign",1,(b,)) init_program(1) minimize(0,((a,1),(b,2))) output_atom(a) output_atom(b) output_term(x,(__aux,)) project((a,)) rule(0,(),(__aux,neg(__aux))) rule(0,(__aux,),()) rule(0,(__aux,),(a,b)) rule(0,(a,),(theory(a,(elem((+(1,2),"test"),(a,b)),)),)) rule(0,(b,),(theory(b(3),(),=,17),)) rule(1,(a,b),(__aux,)) weight_rule(0,(__aux,),1,((a,1),(b,1)))
UNKNOWN
