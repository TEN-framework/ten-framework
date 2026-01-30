# ESP-DSP LyraT Board audio processing application

The demo applications are developed for the ESP32-LyraT development board and demonstrate the usage of IIR filters from the ESP-DSP library.
This example showcases how to use IIR filter functionality to process audio stream data.
To hear the audio please connect headphones or speakers to the ESP32-LyraT audio output.
The example performs the following steps:

1. Read samples from a file
    * read samples from file
    * write audio samples to the triple buffer
2. Process audio samples
    * process volume/bass/treble
    * apply digital limiter to the audio data
    * control audio buffer overflow
3. Pass samples to the audio codec 

To control the volume/bass/treble, please select the value by press 'Set' button and adjust the value by '+/-' buttons.

## The Triple Audio Buffer

In audion processing, it's possible to have situation when one task write data to the processing buffer (processing task), and another task read data from the same buffer, and still in reading process.
Triple buffering is a technique used in audio processing to minimize latency and ensure smooth playback. It involves using three buffers to store audio data. Here's a description of how it works:

* Buffer A: This buffer holds the audio data that is currently being processed by the audio system. It is typically filled with samples from the audio source and processed in real-time.

* Buffer B: When Buffer A is full, the audio system begins reading from Buffer A and starts processing the data. At the same time, Buffer B is filled with new audio samples from the source.

* Buffer C: Once Buffer B is full, the audio system switches its attention to Buffer B and continues processing data. Buffer C is then filled with the latest audio samples.

The cycle continues, with the audio system always processing the data from a buffer while the other two buffers are being filled. This approach helps ensure a continuous and uninterrupted audio playback, as there is always a buffer ready to be processed. It reduces the chances of audio glitches or dropouts caused by delays in reading or processing the audio data.

Triple buffering is particularly useful when working with real-time audio processing applications, where low latency and uninterrupted playback are crucial.

## Audio Processing Flow

The audio processing flow contains next blocks:
1. Audio processing task
    * Read data from file and store to the triple buffer
    * Read from triple buffer and concert data from int16_t to float
    * Process bass
    * Process treble
    * Process volume
    * Apply digital limiter
    * Convert data from float to int16_t and store to the triple buffer
2. Audio output task
    * Write data from triple buffer to audio codec
3. Buttons control task
    * React on buttons and adjust the control values
    * Calculates IIR filter coefficients

## How to use the example

### Hardware required

This example require LyraT development board.

### Configure the project

Under Component Config ---> DSP Library ---> DSP Optimization, it's possible to choose either the optimized or ANSI implementation, to compare them.

### Build and flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace PORT with serial port name):

```
idf.py flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.
