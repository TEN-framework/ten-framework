# Matrix Operations Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example demonstrates how to use Mat class functionality from esp-dsp library. Example does the following steps:

1. Initialize a matrix A and matirx x
2. Calculate matrix b: b = A*x
3. Find roots x1_: A*x1_ = b, with different methods
4. Print result

## How to use example

### Hardware required

This example does not require any special hardware, and can be run on any common development board.

### Configure the project

Under Component Config ---> DSP Library ---> DSP Optimization, it's possible to choose either the optimized or ANSI implementation, to compare them.

### Build and flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace PORT with serial port name):

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example output

Here is an typical example console output. 

```
I (215) main: Start Example.
I (215) main: Original vector x:
0
1
2
I (215) main: Solve result:
0
1
2
I (215) main: Roots result:
0
1
2
I (215) main: End Example.

```
