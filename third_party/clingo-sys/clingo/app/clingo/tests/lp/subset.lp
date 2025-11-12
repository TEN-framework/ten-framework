#heuristic p(1..2). [1,false]
#heuristic p(2..3). [2@1,true]
#heuristic p(3..4). [1,sign]
#heuristic p(4..5). [2@3,level]
#heuristic p(5..6). [3@4,factor]
#heuristic p(6..7). [3@4,init]

3 { p(1..10) }.
2 { p(1..5) }.
4 { p(3..8) }.
2 { p(7..9) }.
3 { p(2..9) }.
