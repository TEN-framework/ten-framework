1 { p; t }   :- 1 { r; s; not t } 2.
  { q; r } 1 :- 1 { p; t }.
    s        :- not q, not r.

#minimize { 1,p:p; 1,q:q; 1,r:r; 1,s:s }.
