Migrating to an auto generated psa_crypto_driver_wrappers.c file
================================================================

**This is a specification of work in progress. The implementation is not yet merged into Mbed TLS.**

This document describes how to migrate to the auto generated psa_crypto_driver_wrappers.c file.
It is meant to give the library user migration guidelines while the Mbed TLS project tides over multiple minor revs of version 1.0, after which this will be merged into psa-driver-interface.md.

## Introduction

The design of the Driver Wrappers code generation is based on the design proposal https://github.com/Mbed-TLS/mbedtls/pull/5067
During the process of implementation there might be minor variations wrt versioning and broader implementation specific ideas, but the design remains the same.

## Prerequisites

Python3 and Jinja2 rev 2.10.1

## Feature Version

1.0

### What's critical for a migrating user

The Driver Wrapper auto generation project is designed to use a python templating library ( Jinja2 ) to render templates based on drivers that are defined using a Driver descrioption JSON file(s).

While that is the larger goal, for version 1.0 here's what's changed

#### What's changed

(1) psa_crypto_driver_wrappers.c will from this point on be auto generated.
(2) The auto generation is based on the template file at scripts/data_files/driver_templates/psa_crypto_driver_wrappers.c.jinja.
(3) So while all driver wrapper templating support is yet to come in, the library user will need to patch into the template file as needed, this could be read as replacing the template file with the current psa_crypto_driver_wrappers.c file maintained by the library user.
