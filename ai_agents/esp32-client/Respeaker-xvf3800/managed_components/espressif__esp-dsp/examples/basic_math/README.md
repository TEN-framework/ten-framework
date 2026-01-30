# Basic Math Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example demonstrates how to use basic math functions from esp-dsp library. Example does the following steps:

1. Initialize the library
2. Initialize input signals with 1024 samples
3. Apply window to input signal by standard C loop.
4. Calculate FFT for 1024 complex samples and show the result
5. Show results on the plots
6. Apply window to input signal by basic math functions dsps_mul_f32 and dsps_mulc_f32.
7. Calculate FFT for 1024 complex samples
8. Show results on the plots

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

```bash
I (132) main: *** Start Example. ***
I (132) main: *** Multiply tone signal with Hann window by standard C loop. ***
I (152) view: Data min[432] = -173.749878, Data max[205] = 23.849705
 ________________________________________________________________
0                         |                                      |
1                         |                                      |
2                         |                                      |
3                         ||                                     |
4                        | |                                     |
5                       ||  |                                    |
6                    |||     ||                                  |
7               |||||          ||||                              |
8|||||||||||||||                   ||||||                        |
9                                        |||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (162) view: Plot: Length=512, min=-120.000000, max=40.000000
I (162) main: *** Multiply tone signal with Hann window by esp-dsp basic math functions. ***
I (162) view: Data min[432] = -173.749878, Data max[205] = 23.849705
 ________________________________________________________________
0                         |                                      |
1                         |                                      |
2                         |                                      |
3                         ||                                     |
4                        | |                                     |
5                       ||  |                                    |
6                    |||     ||                                  |
7               |||||          ||||                              |
8|||||||||||||||                   ||||||                        |
9                                        |||||||||||||||||||||||||
 0123456789012345678901234567890123456789012345678901234567890123
I (172) view: Plot: Length=512, min=-120.000000, max=40.000000
I (172) main: *** End Example. ***
```
