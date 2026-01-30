# ESP-DSP M5Stack Code S3 3d graphics demo application

The demo is developed for [M5Stack Core S3 board](https://docs.m5stack.com/en/core/CoreS3) development board and is demonstrating the usage of matrices with `ESP-DSP` `Mat` class, Kalman filter and basic 3D graphics. 
The demo is showing the basic use case of bmi270 and bmm150 sensors with esp-idf.

The 3D Graphics demo displays a 2D graphics, converted to 3D as a 3D rotating object, on the development board's display. 

## Running the demo

To start the demo, run the following command:

    idf.py build flash monitor

The expected output is the following:

I (2097) ili9341: LCD panel create success, version: 1.2.0
I (2367) 3d-kalman: bmi270 ChipID = 0x24 (should be 0x24), err = 00
I (2377) 3d-kalman: bmm150 chip ID = 0x32 (should be 0x32), err = 00
I (2387) 3d-kalman: bmi270 status = 01
I (3267) 3d-kalman: bmi270 status = 01
I (3277) 3d-kalman: bmi270 initialization is done
I (4277) 3d-kalman: Showing 3D image
I (4277) main_task: Returned from app_main()

Note, that the first line `Selected 3D image` from the expected output depends on the user's Kconfing menu selection
