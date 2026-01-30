# ESP-DSP ESP32-Azure IoT kit Kalman filter demo application

The demo is developed for [ESP32-Azure IoT kit](https://github.com/espressif/esp-bsp/tree/master/esp32_azure_iot_kit) development board and is demonstrating the usage of matrices with `ESP-DSP` `Mat` class, Kalman filter and basic 3D graphics.

The Kalman filter demo displays a 2D graphics, converted to 3D as a 3D object, on the development board's display. The 3D object follows the movements of the development board, where the Kalman filter is used for processing the output signals of the IMU sensors accommodated on the development board. The 3D object rotation is calculated by the Kalman filter class methods. All 3 IMU sensors present on the dev board (accelerometer, gyroscope and magnetometer) are used for sensing the development board's position.


If the board is inactive (no, or very low rotation is detected) for a set period of time, the demo enters an "Idle" state, in which a 3D rotating object is displayed. Once a certain set level of the board's rotation is detected, the demo enters a normal, "Active", state.

For the project settings, run the menuconfig using the following command: 

    idf.py mencuonfig

In the menuconfig's menu item `Demo user configuration` select which 3D object to display. It's either 3D cube, or ESP logo, or a user-defined graphics. Getting the user-defined 3D object is described in the [3D Graphics demo](../3d_graphics)

## Kalman filter
#### Calibration

The filter must be calibrated before the first run, which takes several minutes. But the calibration process before each run can be omitted by calibrating the filter once, saving Kalman's filter state vectors to the NVS, and loading those vectors back into the Kalman filter before the run. In addition, every 5 minutes a current state vectors are saved into the flash memory.

## Running the demo

To start the demo, run the following command:

    idf.py build flash monitor

The expected output is the following:

    I (589) Kalman filter demo: Selected 3D image - 3D cube
    I (590) Kalman filter demo: Filter state vectors present in the NVS
    I (592) Kalman filter demo: Loading state vectors into the filter structure
    I (604) Kalman filter demo: State vectors loaded from the NVS
    I (606) Barometer: disabled
    I (619) Board status: board put to active mode
    I (95780) Board status: board put to idle mode
    I (300619) Kalman filter demo: State vectors saved to NVS

Note, that the first line `Selected 3D image` from the expected output depends on the user's Kconfing menu selection

To start the demo and run the initial Kalman filter calibration, one must erase the flash memory, to remove the previously stored Kalman filter's state vectors. To do so, run the following command:

    idf.py erase_flash build flash monitor

The expected output is the following:

    I (592) Kalman filter demo: Selected 3D image - 3D cube
    I (595) Kalman filter demo: Filter state vectors not present in the NVS
    I (595) Kalman filter demo: Starting Kalman filter calibration loop
    I (100699) Kalman filter demo: Exiting Kalman filter calibration loop
    I (100894) Kalman filter demo: Estimated gyroscope bias error [deg/sec]: -0.020715	-0.000431	-0.022452
    I (100900) Kalman filter demo: State vectors saved to the NVS
    I (100900) Barometer: disabled
    I (100911) Board status: board put to active mode
    I (196072) Board status: board put to idle mode
    I (400912) Kalman filter demo: State vectors saved to NVS

<div align="center">
  <img src= "applications/azure_board_apps/apps/kalman_filter/kalman_filter.gif">
</div>