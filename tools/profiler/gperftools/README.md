# Profiler with gperftools

## Introduction

This document describes how to use gperftools to profile the performance of a
program especially for heap memory usage. Besides, it also describes how to
analyze the profile data with given tools.

## Preparation

### Install gperftools

```bash
apt update && apt install -y google-perftools

# Create a symbolic link for libtcmalloc.so
ln -s /usr/lib/x86_64-linux-gnu/libtcmalloc.so.4 /usr/lib/libtcmalloc.so
```

### Profiling with gperftools

```bash
# Profile the whole process runtime
export LD_PRELOAD=/usr/lib/libtcmalloc.so

# All dumped heap files will be stored in /.../heap
export HEAPPROFILE=/.../heap

# The heap profile will be dumped every 30 seconds
export HEAP_PROFILE_TIME_INTERVAL=30

exec /.../program
```

### Analyze the profile data

Combined the heap profiles with symbols and convert to raw format

```bash
python3 dump_heap_files_to_raw.py -heap_dir=/.../heap -bin=/.../program -raw=/.../raw
```

Convert the raw format to human readable text format

```bash
python3 dump_raw_files_to_text.py -raw_dir=/.../raw -text_dir=/.../text
```

Compare two heap profiles

```bash
python3 compare_heaps.py -base_file=/.../raw/heap.0001.raw -target_file=/.../raw/heap.0002.raw -output_file=/.../diff.pdf -output_type=pdf
```

Analyze all heap profiles and generate an excel file

```bash
python3 dump_heap_info_to_excel.py -heap_dir=/.../heap -bin=/.../program -output=/.../heap.xlsx
```
