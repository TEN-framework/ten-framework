dim(1..16).

barrier( 2, 1, 1,0).  barrier(13,11, 1,0).  barrier( 9, 7,0, 1).
barrier(10, 1, 1,0).  barrier(11,12, 1,0).  barrier(11, 7,0, 1).
barrier( 4, 2, 1,0).  barrier(14,13, 1,0).  barrier(14, 7,0, 1).
barrier(14, 2, 1,0).  barrier( 6,14, 1,0).  barrier(16, 9,0, 1).
barrier( 2, 3, 1,0).  barrier( 3,15, 1,0).  barrier( 2,10,0, 1).
barrier(11, 3, 1,0).  barrier(10,15, 1,0).  barrier( 5,10,0, 1).
barrier( 7, 4, 1,0).  barrier( 4,16, 1,0).  barrier( 8,10,0,-1).
barrier( 3, 7, 1,0).  barrier(12,16, 1,0).  barrier( 9,10,0,-1).
barrier(14, 7, 1,0).  barrier( 5, 1,0, 1).  barrier( 9,10,0, 1).
barrier( 7, 8, 1,0).  barrier(15, 1,0, 1).  barrier(14,10,0, 1).
barrier(10, 8,-1,0).  barrier( 2, 2,0, 1).  barrier( 1,12,0, 1).
barrier(11, 8, 1,0).  barrier(12, 3,0, 1).  barrier(11,12,0, 1).
barrier( 7, 9, 1,0).  barrier( 7, 4,0, 1).  barrier( 7,13,0, 1).
barrier(10, 9,-1,0).  barrier(16, 4,0, 1).  barrier(15,13,0, 1).
barrier( 4,10, 1,0).  barrier( 1, 6,0, 1).  barrier(10,14,0, 1).
barrier( 2,11, 1,0).  barrier( 4, 7,0, 1).  barrier( 3,15,0, 1).
barrier( 8,11, 1,0).  barrier( 8, 7,0, 1).  

available_target(red   , moon  ,  5 , 2).
available_target(red   , sun   , 15,  2).
available_target(green , sun   ,  2,  3).
available_target(blue  , star  , 12,  3).
available_target(yellow, star  ,  7,  4).
available_target(blue  , saturn,  4,  7).
available_target(green , moon  , 14,  7).
available_target(yellow, saturn, 11,  8).
available_target(yellow, moon  ,  5, 10).
available_target(green , star  ,  2, 11).
available_target(red   , star  , 14, 11).
available_target(green , saturn, 11, 12).
available_target(yellow, star  , 15, 13).
available_target(blue  , star  ,  7, 14).
available_target(red   , saturn,  3, 15).
available_target(blue  , moon  , 10, 15).

robot(red;green;blue;yellow).
initial_pos(red   ,1 , 1).
initial_pos(blue  ,1 ,16).
initial_pos(green ,16, 1).
initial_pos(yellow,16,16).
