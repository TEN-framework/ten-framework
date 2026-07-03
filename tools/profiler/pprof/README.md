# Profiler with pprof

## Introduction

This document describes how to use pprof to profile the performance of a
go program especially for heap memory usage. Besides, it also describes how to
analyze the profile data with given tools.

## Preparation

### Install pprof

```bash
go install github.com/google/pprof@latest
```

### Profiling with pprof

The simplest way to profile a go program is to use the pprof_app_go as your TEN app. When the app starts, it will read environment variables to determine whether to start the pprof server and the heap profiler.

```bash
# Heap profiles will be stored in /data/prof
export TEN_HEAP_DUMP_DIR=/data/prof

# The heap profile will be dumped every 30 seconds
export HEAP_PROFILE_TIME_INTERVAL=30

# The pprof server will listen on port 6060
export TEN_PROFILER_SERVER_PORT=6060
```

### Analyze the profile data

Dump the heap profiles to text format

```bash
python3 dump_heap_files_to_text.py -heap_dir=/data/prof -text_dir=/data/text
```

Analyze all heap profiles and generate an excel file

```bash
python3 dump_heap_info_to_excel.py -heap_dir=/data/prof -output=/data/heap.xlsx
```
