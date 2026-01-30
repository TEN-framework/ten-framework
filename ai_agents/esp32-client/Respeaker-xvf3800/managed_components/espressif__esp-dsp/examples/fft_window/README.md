# FFT Window Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example demonstrates how to use Window and FFT functionality from esp-dsp library. Example does the following steps:

1. Initialize the library
2. Initialize input signals with 1024 samples
3. Apply window to input signal.
4. Calculate FFT for 1024 complex samples
5. Apply bit reverse operation for output complex vector
6. Split one complex FFT output spectrum to two real signal spectrums 
7. Show results on the plots

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
I (128) main: Start Example.
W (128) main: Hann Window
I (128) view: Data min[256] = -inf, Data max[1] = 24.086628
 ________________________________________________________________
0|                                                               |
1|                                                               |
2|                                                               |
3|                                                               |
4|                                                               |
5 |                                                              |
6  |                                                             |
7   |||||                                                        |
8        |||||||||||||||                                         |
9                       ||||||||||||||||||||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (138) view: Plot: Length=512, min=-120.000000, max=40.000000
W (138) main: Blackman Window
I (148) view: Data min[355] = -165.295654, Data max[1] = 24.083012
 ________________________________________________________________
0|                                                               |
1|                                                               |
2|                                                               |
3|                                                               |
4|                                                               |
5|                                                               |
6 |                                                              |
7  |||                                                           |
8     |||||||||                                                  |
9              |||||||||||||||||||||||||||||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (158) view: Plot: Length=512, min=-120.000000, max=40.000000
W (158) main: Blackman-Harris Window
I (168) view: Data min[128] = -inf, Data max[1] = 23.874702
 ________________________________________________________________
0|                                                               |
1|                                                               |
2|                                                               |
3|                                                               |
4|                                                               |
5|                                                               |
6|                                                               |
7||                                                              |
8| ||||                                                          |
9      |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (178) view: Plot: Length=512, min=-120.000000, max=40.000000
W (178) main: Blackman-Nuttall Window
I (188) view: Data min[128] = -inf, Data max[1] = 23.890663
 ________________________________________________________________
0|                                                               |
1|                                                               |
2|                                                               |
3|                                                               |
4|                                                               |
5|                                                               |
6|                                                               |
7 ||                                                             |
8   |||| |                                                       |
9       | ||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (198) view: Plot: Length=512, min=-120.000000, max=40.000000
W (198) main: Nuttall Window
I (208) view: Data min[203] = -175.147400, Data max[1] = 23.858671
 ________________________________________________________________
0|                                                               |
1|                                                               |
2|                                                               |
3|                                                               |
4|                                                               |
5|                                                               |
6|                                                               |
7||                                                              |
8  |||                                                           |
9     ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (218) view: Plot: Length=512, min=-120.000000, max=40.000000
W (218) main: Flat-Top Window
I (228) view: Data min[256] = -inf, Data max[1] = 22.490753
 ________________________________________________________________
0|                                                               |
1|                                                               |
2|                                                               |
3|                                                               |
4|                                                               |
5|                                                               |
6|                                                               |
7 ||                                                             |
8   |||||                                                        |
9        |||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (238) view: Plot: Length=512, min=-120.000000, max=40.000000
I (238) main: End Example.
```
