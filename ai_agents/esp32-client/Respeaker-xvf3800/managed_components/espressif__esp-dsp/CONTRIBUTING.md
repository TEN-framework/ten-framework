# Contributing

Contributions to ESP-DSP project in the form of pull requests, bug reports, and feature requests are welcome!

This document covers various topics related to contributions to the ESP-DSP projects. Please read it if you plan to submit a PR!

## CLA

We require accepting the contributor's license agreement for all pull requests. When opening a pull request the first time you will be prompted to sign the CLA by the [CLA Assistant](https://cla-assistant.io/) service.

## Large-scale Changes

If you'd like to propose a change to the existing APIs or a large-scale refactoring of the implementation, we recommend opening an issue first to discuss this.

## Pre-commit hooks

ESP-DSP project uses [pre-commit hooks](https://pre-commit.com/) to perform code formatting and other checks when you run `git commit`.

To install pre-commit hooks, run `pip install pre-commit && pre-commit install`. 

## Updating the Benchmarks Table

The benchmarks table [esp-dsp-benchmarks.rst](docs/esp-dsp-benchmarks.rst) contains benchmarks for ESP32 and ESP32-S3 CPUs. The benchmarks are collected with compiler optimizations for speed (-Os) and for size (-O2). This table may need to be updated if you have changed the implementations of some of the functions.

To build this table:

- Build the test application in the `test_app` directory.
- Run "DSP Benchmarks data" test and copy the output into the CSV file [docs/esp_bm_results.csv](docs/esp_bm_results.csv).
- Run the [docs/build_bm_table.py](docs/build_bm_table.py) script to update the benchmarks table [esp-dsp-benchmarks.rst](docs/esp-dsp-benchmarks.rst).

If you have added new functions, modify the [benchmarks test](test/test_dsp.c) to call these new functions.

## Supported IDF Versions

The component is expected to be usable with multiple supported IDF versions. You can find the list in the [CI workflow file](.gitlab-ci.yml).

Note that for compatibility reasons, the component has to support the legacy GNU Make build system which was present up until ESP-IDF v5.0. Remember to add or update `component.mk` and `Makefile` files when adding or changing examples. GNU Make support can be removed from ESP-DSP when IDF v4.4 maintenance period expires.

## Releasing a new version

Maintainers should follow the steps below to release a new version of ESP-DSP component. Assuming the new version is `vX.Y.Z`:

1. Ensure you are on the latest `master` branch, then create a new branch:
   ```bash
   git checkout master
   git pull --ff-only origin master
   git checkout -b update_version_vX.Y.Z
   ```
1. Update the version in [idf_component.yml](idf_component.yml):
   ```yml
   version: "X.Y.Z"
   ```
1. Commit the changes:
   ```bash
   git add idf_component.yml
   git commit -s -m "version: update to vX.Y.Z"
   ```
1. Create the new tag:
   ```bash
   git tag -s -a -m "vX.Y.Z" vX.Y.Z
   ```
1. Push the tag and the branch to the internal repository:
   ```bash
   git push origin update_version_vX.Y.Z
   git push origin vX.Y.Z
   ```
1. Create the merge request, get it reviewed and merged.
1. CI will automatically push the tag to Github and will upload the new version to the IDF Component Registry.
1. Go to https://github.com/espressif/esp-dsp/releases and create a release from the tag vX.Y.Z.
1. Write the release notes and publish the release.
