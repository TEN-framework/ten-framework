# FFT Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example demonstrates how to use FFT functionality from esp-dsp library. Example does the following steps:

1. Initialize the library
2. Initialize input signals with 1024 samples: one 0 dB, second with -20 dB
3. Combine two signals as one complex input signal and apply window to input signals paar.
4. Calculate FFT for 1024 complex samples
5. Apply bit reverse operation for output complex vector
6. Split one complex FFT output spectrum to two real signal spectrums 
7. Show results on the plots
8. Show execution time of FFT

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
I (59) main: Start Example.
W (89) main: Signal x1
I (89) view: Data min[495] = -162.760925, Data max[164] = 23.938747
 ________________________________________________________________
0                                                                |
1                    |                                           |
2                    |                                           |
3                    |                                           |
4                    |                                           |
5                    |                                           |
6                   | |                                          |
7                   | |                                          |
8                  || ||                                         |
9||||||||||||||||||     ||||||||||||||||||||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (159) view: Plot: Length=512, min=-60.000000, max=40.000000
W (169) main: Signal x2
I (169) view: Data min[502] = -164.545135, Data max[205] = 3.857752
 ________________________________________________________________
0                                                                |
1                                                                |
2                                                                |
3                         |                                      |
4                         |                                      |
5                         |                                      |
6                         |                                      |
7                         ||                                     |
8                        | |                                     |
9||||||||||||||||||||||||   ||||||||||||||||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (249) view: Plot: Length=512, min=-60.000000, max=40.000000
W (249) main: Signals x1 and x2 on one plot
I (259) view: Data min[505] = -159.215271, Data max[164] = 23.938747
 ________________________________________________________________
0                                                                |
1                    |                                           |
2                    |                                           |
3                    |    |                                      |
4                    |    |                                      |
5                    |    |                                      |
6                   | |   |                                      |
7                   | |   ||                                     |
8                  || || | |                                     |
9||||||||||||||||||     |   ||||||||||||||||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (339) view: Plot: Length=512, min=-60.000000, max=40.000000
I (339) main: FFT for 1024 complex points take 140472 cycles
I (349) main: End Example.
```
