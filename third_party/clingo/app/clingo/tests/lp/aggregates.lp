arg(s3). sat(s3):-ass(s3,Y_s3),lt(Y_s3,Z_s3),V_t_0=0, V_3_1=-V_t_0, V_t_2=0, V_2_1=-2*V_t_2+V_3_1, dom(V_t_3), V_t_3 #sum{Z_s3,s3}V_t_3, V_1_1=1*V_t_3+V_2_1, V_t_4=0, V_0_1=2*V_t_4+V_1_1, dom(V_1), V_1 #sum{1:V_0_1>0} V_1, V_1=1.
inv(s3):-ass(s3,Y_s3),lt(Y_s3,Z_s3),V_t_0=0, V_3_1=-V_t_0, V_t_2=0, V_2_1=-2*V_t_2+V_3_1, dom(V_t_3), V_t_3 #sum{Z_s3,s3} V_t_3, V_1_1=1*V_t_3+V_2_1, V_t_4=0, V_0_1=2*V_t_4+V_1_1, dom(V_1), V_1 #sum{1:V_0_1>0}V_1, V_1=0.
sattwo(s3):-asstwo(s3,Y_s3),lt(Y_s3,Z_s3),V_t_0=0, V_3_1=-V_t_0, V_t_2=0, V_2_1=-2*V_t_2+V_3_1, dom(V_t_3), V_t_3#sum{Z_s3,s3}V_t_3, V_1_1=1*V_t_3+V_2_1, V_t_4=0, V_0_1=2*V_t_4+V_1_1, dom(V_1), V_1 #sum{1:V_0_1>0}V_1, V_1=1.
invtwo(s3):-asstwo(s3,Y_s3),lt(Y_s3,Z_s3),V_t_0=0, V_3_1=-V_t_0, V_t_2=0, V_2_1=-2*V_t_2+V_3_1, dom(V_t_3), V_t_3#sum{Z_s3,s3}V_t_3, V_1_1=1*V_t_3+V_2_1, V_t_4=0, V_0_1=2*V_t_4+V_1_1, dom(V_1), V_1 #sum{1:V_0_1>0}V_1, V_1=0.
arg(s2).
sat(s2):-V_t_5=0, V_3_6=-V_t_5, V_t_7=0, V_2_6=-2*V_t_7+V_3_6, V_t_8=0, V_1_6=1*V_t_8+V_2_6, V_t_9=0, V_0_6=2*V_t_9+V_1_6, dom(V_6), V_6#sum{1:V_0_6>0}V_6, V_6=1.
inv(s2):-V_t_5=0, V_3_6=-V_t_5, V_t_7=0, V_2_6=-2*V_t_7+V_3_6, V_t_8=0, V_1_6=1*V_t_8+V_2_6, V_t_9=0, V_0_6=2*V_t_9+V_1_6, dom(V_6), V_6#sum{1:V_0_6>0}V_6, V_6=0.
sattwo(s2):-V_t_5=0, V_3_6=-V_t_5, V_t_7=0, V_2_6=-2*V_t_7+V_3_6, V_t_8=0, V_1_6=1*V_t_8+V_2_6, V_t_9=0, V_0_6=2*V_t_9+V_1_6, dom(V_6), V_6#sum{1:V_0_6>0}V_6, V_6=1.
invtwo(s2):-V_t_5=0, V_3_6=-V_t_5, V_t_7=0, V_2_6=-2*V_t_7+V_3_6, V_t_8=0, V_1_6=1*V_t_8+V_2_6, V_t_9=0, V_0_6=2*V_t_9+V_1_6, dom(V_6), V_6#sum{1:V_0_6>0}V_6, V_6=0.
arg(s1).
sat(s1):-ass(s2,Y_s2),lt(Y_s2,Z_s2),ass(s3,Y_s3),lt(Y_s3,Z_s3),dom(V_t_10), V_t_10#sum{Z_s2,s2}V_t_10, V_3_11=-V_t_10, V_t_12=0, V_2_11=-2*V_t_12+V_3_11, dom(V_t_13), V_t_13 #sum{Z_s3,s3}V_t_13, V_1_11=1*V_t_13+V_2_11, V_t_14=0, V_0_11=2*V_t_14+V_1_11, dom(V_11), V_11#sum{1:V_0_11>0}V_11, V_11=1.
inv(s1):-ass(s2,Y_s2),lt(Y_s2,Z_s2),ass(s3,Y_s3),lt(Y_s3,Z_s3),dom(V_t_10), V_t_10#sum{Z_s2,s2}V_t_10, V_3_11=-V_t_10, V_t_12=0, V_2_11=-2*V_t_12+V_3_11, dom(V_t_13), V_t_13 #sum{Z_s3,s3}V_t_13, V_1_11=1*V_t_13+V_2_11, V_t_14=0, V_0_11=2*V_t_14+V_1_11, dom(V_11), V_11#sum{1:V_0_11>0}V_11, V_11=0.
sattwo(s1):-asstwo(s2,Y_s2),lt(Y_s2,Z_s2),asstwo(s3,Y_s3),lt(Y_s3,Z_s3),dom(V_t_10), V_t_10#sum{Z_s2,s2}V_t_10, V_3_11=-V_t_10, V_t_12=0, V_2_11=-2*V_t_12+V_3_11, dom(V_t_13), V_t_13#sum{Z_s3,s3}V_t_13, V_1_11=1*V_t_13+V_2_11, V_t_14=0, V_0_11=2*V_t_14+V_1_11, dom(V_11), V_11#sum{1:V_0_11>0}V_11, V_11=1.
invtwo(s1):-asstwo(s2,Y_s2),lt(Y_s2,Z_s2),asstwo(s3,Y_s3),lt(Y_s3,Z_s3),dom(V_t_10), V_t_10#sum{Z_s2,s2}V_t_10, V_3_11=-V_t_10, V_t_12=0, V_2_11=-2*V_t_12+V_3_11, dom(V_t_13), V_t_13#sum{Z_s3,s3}V_t_13, V_1_11=1*V_t_13+V_2_11, V_t_14=0, V_0_11=2*V_t_14+V_1_11, dom(V_11), V_11#sum{1:V_0_11>0}V_11, V_11=0.
saturate:-ass(s3,X_1),asstwo(s3,X_1),ass(s2,X_2),asstwo(s2,X_2),ass(s1,X_3),asstwo(s1,X_3).
dom(0..1).
lt(u,0).lt(u,1).lt(1,1).lt(0,0).
ass(S,0):-not ass(S,1),not ass(S,u),arg(S).
ass(S,1):-not ass(S,u),not ass(S,0),arg(S).
ass(S,u):-not ass(S,0),not ass(S,1),arg(S).
:-arg(S),ass(S,1),inv(S).
:-arg(S),ass(S,0),sat(S).
asstwo(S,0):-ass(S,0).
asstwo(S,1):-ass(S,1).
asstwo(S,0)|asstwo(S,1)|asstwo(S,u):-ass(S,u).
saturate:-arg(S),asstwo(S,1),invtwo(S).
saturate:-arg(S),asstwo(S,0),sattwo(S).
asstwo(S,0):-arg(S),ass(S,u),saturate.
asstwo(S,1):-arg(S),ass(S,u),saturate.
asstwo(S,u):-arg(S),ass(S,u),saturate.
sattwo(S):-arg(S),ass(S,u),saturate.
:-not saturate.
#show ass/2.
       
:-ass(s1, 0),ass(s2, 0),ass(s3, 0).
:-ass(s1, 1),ass(s2, 0),ass(s3, 1).
       
%unfounded: saturate, asstwo(s1,0), asstwo(s1,0)