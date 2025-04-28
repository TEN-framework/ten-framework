# Preparation

## Install VOSK SDK

Since the vosk SDK (which includes a header file and a library file) is relatively large, it is not included in this extension by default. You need to manually download the vosk SDK from [https://github.com/alphacep/vosk-api/releases](https://github.com/alphacep/vosk-api/releases), place the header file (`vosk_api.h`) in the `include/` directory, and place the library file (`libvosk.so`) in the `lib_private/` directory.

## Install VOSK Models

Download the desired model from [https://alphacephei.com/vosk/models](https://alphacephei.com/vosk/models), extract it, and place it in the `models/` directory.
