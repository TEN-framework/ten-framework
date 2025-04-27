Simple example to show how to use program parts and externals.

Examples
========

    $ python app.py
    Exmaple 1:
      loading files:
      - chemistry.lp
      grounding:
      - base
      solutions:
      - a(1) a(2)
    
    Exmaple 2:
      loading files:
      - chemistry.lp
      grounding:
      - acid(42)
      solutions:
      - b(42)
    
    Exmaple 3:
      loading files:
      - chemistry.lp
      - external.lp
      grounding:
      - base
      - acid(42)
      assigning externals:
      - d(1,42)=True
      solutions:
      - a(1) a(2) b(42) c(1,42) c(2,42) d(1,42) e(1,42)
