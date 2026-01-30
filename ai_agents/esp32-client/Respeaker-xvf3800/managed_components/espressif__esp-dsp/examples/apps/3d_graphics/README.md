# ESP-DSP ESP32-Azure IoT kit 3d graphics demo application

The demo is developed for [ESP32-Azure IoT kit](https://github.com/espressif/esp-bsp/tree/master/esp32_azure_iot_kit) development board and is demonstrating the usage of matrices with `ESP-DSP` `Mat` class, Kalman filter and basic 3D graphics.

The 3D Graphics demo displays a 2D graphics, converted to 3D as a 3D rotating object, on the development board's display. Button press changes the rotation direction of the 3D object. Run the menuconfig using the following command: 

    idf.py mencuonfig

In the menuconfig's menu item `Demo user configuration` select which 3D object to display. It's either a 3D cube, or ESP logo, or a user-defined graphics. Getting the user-defined graphics is described in an [example](../../graphics/img_to_3d_matrix/example/)

## Running the demo

To start the demo, run the following command:

    idf.py build flash monitor

The expected output is the following:

    I (570) 3D image demo: Selected 3D image - ESP Logo
    I (570) 3D image demo: Showing ESP text
    I (6730) 3D image demo: Showing 3D image

Note, that the first line `Selected 3D image` from the expected output depends on the user's Kconfing menu selection

<div align="center">
  <img src= "applications/azure_board_apps/apps/3d_graphics/3d_graphics.gif">
</div>
