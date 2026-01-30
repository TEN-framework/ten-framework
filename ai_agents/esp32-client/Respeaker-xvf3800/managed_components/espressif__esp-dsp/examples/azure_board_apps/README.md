# ESP-DSP ESP32-Azure IoT kit demo applications

The demo applications are developed for ESP32-Azure IoT kit development board and are demonstrating usage of matrices with ESP-DPS Mat class, Kalman filter and basic
3D projection with matrices

### [3D Graphics demo](apps/3d_graphics)
* a simple rotation of a selected 3D objects is shown on the display
* demonstrating usage the Mat class, matrices and operations with matrices

<div align="center">
  <img src= "apps/3d_graphics/3d_graphics.gif">
</div>

### [Kalman filter demo](apps/kalman_filter/)
* a selected 3D object is following movements of the development board with IMU sensors
* the sensors outputs are processed by the Kalman filter
* demonstrating usage the Mat class, matrices, operations with matrices and Kalman filter

<div align="center">
  <img src= "apps/kalman_filter/kalman_filter.gif">
</div>

### [3D graphics](graphics/img_to_3d_matrix/example/)

Simple 3D graphics is used in the both demos, to display the 3D on the monochromatic display. The Graphics is using transformation matrices to rotate, scale and translate (move) 3D objects, to show the matrix calculation capabilities of the ESP-DSP repository.

## Used components
* [ESP-DSP](https://github.com/espressif/esp-dsp): matrices, Mat class, Kalman filter
* [ESP-BSP](https://github.com/espressif/esp-bsp): ESP32-Azure IoT kit