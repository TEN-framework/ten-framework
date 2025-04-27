ABI Compatility Tracking
========================

This directory contains files that can be used to generate an ABI compatibility
timeline for libwebsockets. This gives users an idea of where the library has
changed and can be used by the developers to see when incompatible changes have
been introduced and either increase the library SO version or fix the changes.

The tools used are the abi-\* family available at https://github.com/lvc/ and
some example output is here: http://abi-laboratory.pro/tracker/timeline/libuv/

The tools download existing source tarballs and git repository to generate this
data, so past versions are compared and in-development code can be compared as
well.

Although the application is not being included here, FYI the license is dual
LGPL2 / GPL2 at your choice.


Installation
------------

The abi monitoring stuff is now packaged in, eg, fedora, which is a lot
easier than using the helper script.

```
# dnf install abi-tracker vtable-dumper

Otherwise, the author provides an "easy way" to install the various tools he provides:

    git clone https://github.com/lvc/installer
	cd installer
	make prefix=/usr/local target=abi-tracker

It will also list any dependencies that you need to install through normal
means.  (Although in the case of needing "elfutils-libelf-devel", it may
crash during install of vtable-dumper without giving a nice list)


Generating the output
---------------------

Use the `lws-abi-update.sh` script to download the source files, build them and
generate the output html. The output can be deployed to a directory on a web
server for example. Modify the commented line in lws-abi-update.sh to do this.

As it is configured, lws-abi-update.sh will only download new source - ones
that it hasn't built before - so is suitable for use with a cron job.


Viewing the output
------------------

The best place to start looking at the data is the `timeline/libwebsockets`
directory. If your path is on a web server, navigate there, otherwise you could
try:

    lynx timeline/libwebsockets/
