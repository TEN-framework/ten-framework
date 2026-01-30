# ESP-DSP ESP32-S3-BOX-Lite Board demo application

The demo applications are developed for the ESP32-S3-BOX-Lite development board and demonstrate the usage of FFT functions from the ESP-DSP library.
This example showcases how to use FFT functionality to process audio stream data.
The application record sound from two microphones and show the spectrum from them at display as 2D plot.

## Audio Processing Flow

The audio processing flow contains next blocks:
1. Audio processing task
    * Read left and right channel data from the microphone
    * Apply window multiplication to both channels
    * Process FFT and apply bit reverse
    * Split complex spectrum from two channels to two spectrums of two real channels
    * Calculate absolute spectrum and convert it to dB
    * Calculate moving average of the spectrum
2. Image Display Task
    * Read data from the 
    * Write data from triple buffer to audio codec

## How to use the example

Just flash the application to the ESP32-S3-BOX-Lite development board, and play some music around or start to speak to the board microphones.
The display will show the real-time spectrum.
The microphone sensitivity could be adjusted in the code.

### Hardware required

This example does not require any special hardware, and can be run on any common development board.

### Configure the project

Under Component Config ---> DSP Library ---> DSP Optimization, it's possible to choose either the optimized or ANSI implementation, to compare them.

### Build and flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace PORT with serial port name):

```
idf.py flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.
