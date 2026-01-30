# Dot Product Calculation Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example demonstrates how to use dotprod dsps_dotprod_f32 from esp-dsp library. Example does the following steps:

1. Initialize the input arrays
2. Calculate dot product of two arrays
3. Compare results and calculate execution time in cycles.

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
I (55) main: Start Example.
I (55) main: The sum of 101 elements from 0..100 = 5050.000000
I (55) main: Operation for 101 samples took 1381 cycles
I (65) main: End Example.
```
