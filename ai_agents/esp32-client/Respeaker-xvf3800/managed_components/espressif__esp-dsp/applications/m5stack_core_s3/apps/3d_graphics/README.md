# ESP-DSP M5Stack Core s3 board 3d graphics demo application

The demo is developed for [M5Stack Core S3 board](https://docs.m5stack.com/en/core/CoreS3) development board and is demonstrating the usage of matrices with `ESP-DSP` `Mat` class, Kalman filter and basic 3D graphics.

The 3D Graphics demo displays a 2D graphics, converted to 3D as a 3D rotating object, on the development board's display.  

## Running the demo

To start the demo, run the following command:

    idf.py build flash monitor

The expected output is the following:

    I (2103) gpio: GPIO[35]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
    I (2103) ili9341: LCD panel create success, version: 1.2.0
    I (3263) 3D image demo: Showing 3D image

