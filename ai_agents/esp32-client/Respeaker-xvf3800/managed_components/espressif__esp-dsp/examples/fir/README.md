# FIR Filter Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example demonstrates how to use FIR filter functionality from esp-dsp library. Example does the following steps:

1. Initialize the FFT library
2. Initialize input signal
    * 1st Sine wave (f = 0.2Fs)
    * 2nd Sine wave (f = 0.4Fs)
    * Combine the waves 
3. Show input signal
    * Calculate windows coefficients
    * Apply the windowing to the input signal
    * Do the FFT
    * Show the frequency response on a plot
    * Calculate execution performance
4. Show filtered signal
    * Initialize the FIR filter library
    * Calculate Windowed-Sinc coefficients of FIR filter
    * Apply the FIR filter to the input signal
    * Do the FFT
    * Show the frequency response on a plot
    * Calculate execution performance

## How to use the example

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

Here is a typical example of console output. 

```
I (340) main: Start Example.
I (400) view: Data min[388] = -108.419342, Data max[205] = 30.267143
 ________________________________________________________________
0                         |                         |            |
1                         |                        ||            |
2                         ||                       ||            |
3                         ||                       ||            |
4                         ||                       ||            |
5                         ||                       |||           |
6                       || ||                     |   |          |
7||||||||||||||||||||||||    ||||||||||||||||||||||   ||||||||||||
8                                                                |
9                                                                |
 0123456789012345678901234567890123456789012345678901234567890123
I (470) view: Plot: Length=512, min=-120.000000, max=40.000000


I (490) view: Data min[254] = -114.853371, Data max[205] = 27.247583
 ________________________________________________________________
0                                                   |            |
1                                                  |             |
2                                                  |             |
3                                                  | |           |
4                                                  | |           |
5                                                  | |           |
6                         |                     |||  |||         |
7|||||||||||||||||||||||||||||||||||||||||||||||     |  |||||    |
8                                                            |||||
9                                                                |
 0123456789012345678901234567890123456789012345678901234567890123
I (560) view: Plot: Length=256, min=-120.000000, max=40.000000
I (560) main: FIR for 1024 samples and decimation 2 takes 763647 cycles
I (570) main: End Example.

```
