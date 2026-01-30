# IIR Filter Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example demonstrates how to use IIR filters functionality from esp-dsp library. Example does the following steps:

1. Initialize the library
2. Initialize input signal
3. Show LPF filter with Q factor 1
    * Calculate iir filter coefficients
    * Filter the input test signal (delta function)
    * Shows impulse response on the plot
    * Shows frequency response on the plot
    * Calculate execution performance
4. The same for LPF filter with Q factor 10

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
I (58) main: Start Example.
I (58) main: Impulse response of IIR filter with F=0.100000, qFactor=1.000000
I (68) view: Data min[8] = -0.060052, Data max[2] = 0.333517
 ________________________________________________________________
0                                                                |
1                                                                |
2 -                                                              |
3- -                                                             |
4   -------------------------------------------------------------|
5                                                                |
6                                                                |
7                                                                |
8                                                                |
9                                                                |
 0123456789012345678901234567890123456789012345678901234567890123
I (138) view: Plot: Length=128, min=-1.000000, max=1.000000
I (148) view: Data min[511] = -149.983795, Data max[0] = 0.000000
 ________________________________________________________________
0                                                                |
1                                                                |
2-----------------                                               |
3                 ----------                                     |
4                           -------------                        |
5                                        ----------              |
6                                                  -------       |
7                                                         ---    |
8                                                            --  |
9                                                              --|
 0123456789012345678901234567890123456789012345678901234567890123
I (228) view: Plot: Length=512, min=-100.000000, max=0.000000
I (228) main: IIR for 1024 samples take 20276 cycles
I (238) main: Impulse response of IIR filter with F=0.100000, qFactor=10.000000
I (248) view: Data min[7] = -0.453739, Data max[2] = 0.526114
 ________________________________________________________________
0                                                                |
1                                                                |
2 -    -                                                         |
3- -  - -  ---  ---   -    -                                     |
4- -  - -            - ---- -------------------------------------|
5   --   --   --   --                                            |
6                                                                |
7                                                                |
8                                                                |
9                                                                |
 0123456789012345678901234567890123456789012345678901234567890123
I (318) view: Plot: Length=128, min=-1.000000, max=1.000000
I (328) view: Data min[511] = -149.480377, Data max[0] = 0.000000
 ________________________________________________________________
0            --                                                  |
1          --  -                                                 |
2----------     -----                                            |
3                    --------                                    |
4                            ------------                        |
5                                        ----------              |
6                                                  -------       |
7                                                         ---    |
8                                                            --  |
9                                                              --|
 0123456789012345678901234567890123456789012345678901234567890123
I (408) view: Plot: Length=512, min=-100.000000, max=0.000000
I (408) main: IIR for 1024 samples take 17456 cycles
I (418) main: End Example.

```
