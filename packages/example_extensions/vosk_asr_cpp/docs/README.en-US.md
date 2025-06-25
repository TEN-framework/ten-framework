# Vosk ASR C++ Extension

## Overview

Vosk automatic speech recognition extension written in C++ for TEN Framework

## Features

- C++ ASR extension using Vosk for TEN Framework

## Getting Started

### Prerequisites

#### Install VOSK SDK

Since the vosk SDK (which includes a header file and a library file) is relatively large, it is not included in this extension by default. You need to manually download the vosk SDK from [https://github.com/alphacep/vosk-api/releases](https://github.com/alphacep/vosk-api/releases), place the header file (`vosk_api.h`) in the `include/` directory, and place the library file (`libvosk.so`) in the `lib_private/` directory.

#### Install VOSK Models

Download the desired model from [https://alphacephei.com/vosk/models](https://alphacephei.com/vosk/models), extract it, and place it in the `models/` directory.

### Installation

Follow the TEN Framework package installation guidelines.

## Usage

This package can be integrated into TEN applications according to the framework specifications.

## License

This package is part of the TEN Framework project.
