[![Component Registry](https://components.espressif.com/components/espressif/esp-dsp/badge.svg)](https://components.espressif.com/components/espressif/esp-dsp)

# Espressif DSP Library

ESP-DSP is the official DSP library for all Espressif chips. The library contains optimized functions for [ESP32](https://espressif.com/en/products/hardware/esp32/overview), [ESP32-S3](https://espressif.com/en/products/hardware/esp32s3/overview) and [ESP32P4](https://www.espressif.com/en/products/socs/esp32-p4) chips. 

## Overview

ESP-DSP is intended to be used as an [ESP-IDF](https://github.com/espressif/esp-idf) component. For the introduction to ESP-IDF, refer to the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/).

The ESP-DSP library includes implementations of the following functions:
- Matrix multiplication: [reference](https://docs.espressif.com/projects/esp-dsp/en/latest/esp-dsp-apis.html#matrix-operations-apis)
- Dot product: [reference](https://docs.espressif.com/projects/esp-dsp/en/latest/esp-dsp-apis.html#dot-product), [example](https://github.com/espressif/esp-dsp/tree/master/examples/dotprod)
- FFT: [reference](https://docs.espressif.com/projects/esp-dsp/en/latest/esp-dsp-apis.html#fft), [example](https://github.com/espressif/esp-dsp/tree/master/examples/fft)
- IIR: [reference](https://docs.espressif.com/projects/esp-dsp/en/latest/esp-dsp-apis.html#iir), [example](https://github.com/espressif/esp-dsp/tree/master/examples/iir)
- FIR: [reference](https://docs.espressif.com/projects/esp-dsp/en/latest/esp-dsp-apis.html#fir)
- Vector math operations: [reference](https://docs.espressif.com/projects/esp-dsp/en/latest/esp-dsp-apis.html#math)
- Kalman filter: [reference](https://docs.espressif.com/projects/esp-dsp/en/latest/esp-dsp-apis.html#kalman)

Many of the library functions are written in assembly and are optimized for the CPU configuration used in the ESP32. In addition to the optimized implementations, reference implementations written in ANSI C are provided.

Function implementations are provided for single precision floating point (32-bit float), and 16-bit signed integers.

## Documentation

- [ESP-DSP Overview](https://docs.espressif.com/projects/esp-dsp/en/latest/esp-dsp-library.html)
- [ESP-DSP API Reference](https://docs.espressif.com/projects/esp-dsp/en/latest/esp-dsp-apis.html)
- [ESP-DSP Benchmarks](https://docs.espressif.com/projects/esp-dsp/en/latest/esp-dsp-benchmarks.html)

Documentation found in the above links is automatically generated from the contents of this repository. If you find that some information is missing or incomplete, please report an issue.

## Installation and Usage

The ESP-DSP library is a component for the [ESP-IDF build system](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html).

The recommended way to use the component is to install it from the [IDF Component Registry](https://components.espressif.com/components/espressif/esp-dsp).

### Adding ESP-DSP component to an existing project

In the project directory, run:
```bash
idf.py add-dependency "espressif/esp-dsp"
```
This will add the esp-dsp component as a dependency to the `main` component of your project. You can also add it by editing the `idf_component.yml` file manually.

### Downloading ESP-DSP examples

You can download the example projects from the IDF Component Registry website or use the `idf.py create-project-from-example` command. For example:

```bash
idf.py create-project-from-example "espressif/esp-dsp:basic_math"
```

Please refer to the [IDF Component Registry](https://components.espressif.com/components/espressif/esp-dsp) for the download links and the instructions.

You can also use Git to clone this repository and find all the examples in the `examples/` subdirectory. For the list of the examples, please see [README.md](examples/README.md) in the examples directory.

### Building and running ESP-DSP examples

Build, flash and monitor as this is usually done for ESP-IDF projects:

```bash
idf.py -p PORT flash monitor
```

where `PORT` is the UART port name of your development board, such as `/dev/ttyUSB0` or `COM1`. 

Note that you need to set up ESP-IDF before building the project. Refer to the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) if you don't have the environment set up yet.

## Reporting Issues

If you have found an issue in ESP-DSP, or wish to submit an enhancement request, please use the [Issues](https://github.com/espressif/esp-dsp/issues) section on Github.

For general questions related to this library, please use the [esp32.com forum](https://esp32.com/).

## Contributing to ESP-DSP

Please check [CONTRIBUTING.md](CONTRIBUTING.md) if you'd like to contribute to ESP-DSP.

## Copyrights and License

All original source code in this repository is Copyright (C) 2018-2023 Espressif Systems. This source code is licensed under the Apache License 2.0 as described in the file LICENSE.
