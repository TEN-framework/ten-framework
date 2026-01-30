# FFT 4 Real Input Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example demonstrates how to use FFT functionality from esp-dsp library. Example does the following steps:

1. Initialize the library
2. Initialize input signals with 1024 samples: one 0 dB, second with -20 dB
3. Calculate FFT Radix-2 for 1024 complex samples
4. Calculate FFT Radix-4 for 1024 complex samples
5. Apply bit reverse operation for output complex vectors
6. Show results on the plots
7. Show execution time of FFTs

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
I (344) main: Start Example.
W (424) main: Signal x1
I (424) view: Data min[673] = -103.113297, Data max[328] = 20.490950
 ________________________________________________________________
0                                                                |
1                    |                                           |
2                    |                                           |
3                    |                                           |
4                    |                                           |
5                    |                                           |
6                    |                                           |
7                   | |                                          |
8                   | |                                          |
9|||||||||||||||||||   |||||||||||||||||||||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (494) view: Plot: Length=1024, min=-60.000000, max=40.000000
W (504) main: Signal x2
I (504) view: Data min[582] = -103.113297, Data max[328] = 20.490950
 ________________________________________________________________
0                                                                |
1                    |                                           |
2                    |                                           |
3                    |                                           |
4                    |                                           |
5                    |                                           |
6                    |                                           |
7                   | |                                          |
8                   | |                                          |
9|||||||||||||||||||   |||||||||||||||||||||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (584) view: Plot: Length=1024, min=-60.000000, max=40.000000
W (593) main: Difference between signals x1 and x2 on one plot
I (594) view: Data min[0] = 0.000000, Data max[392] = 0.313019
 ________________________________________________________________
0                                                                |
1                                                                |
2                                                                |
3                                                                |
4                                                                |
5                                                                |
6                                                                |
7                                                                |
8----------------------------------------------------------------|
9                                                                |
 0123456789012345678901234567890123456789012345678901234567890123
I (674) view: Plot: Length=1024, min=0.000000, max=40.000000
I (674) main: FFT Radix 2 for 1024 complex points take 168652 cycles
I (684) main: FFT Radix 4 for 1024 complex points take 104665 cycles
I (694) main: End Example.
```
