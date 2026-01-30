# 2D convolution Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example demonstrates how to use 2D convolution dspi_conv_f32 from esp-dsp library. Example does the following steps:

1. Initialize the input arrays
2. Calculate 2D convolution of two images

The example reproduce the Matlab code:

```
A = ones(8);
B = ones(4);
Csame = conv2(A,B, "same")

```


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
I (305) main: Start Example.
I (305) main: 2D Convolution reuslt.
[ 0 ..  8,  0]:   9, 12, 12, 12, 12, 12,  9,  6,
[ 0 ..  8,  1]:  12, 16, 16, 16, 16, 16, 12,  8,
[ 0 ..  8,  2]:  12, 16, 16, 16, 16, 16, 12,  8,
[ 0 ..  8,  3]:  12, 16, 16, 16, 16, 16, 12,  8,
[ 0 ..  8,  4]:  12, 16, 16, 16, 16, 16, 12,  8,
[ 0 ..  8,  5]:  12, 16, 16, 16, 16, 16, 12,  8,
[ 0 ..  8,  6]:   9, 12, 12, 12, 12, 12,  9,  6,
[ 0 ..  8,  7]:   6,  8,  8,  8,  8,  8,  6,  4,
```
