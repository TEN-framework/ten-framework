`msgpack` for C
===================

Version 6.1.0 [![Build Status](https://github.com/msgpack/msgpack-c/workflows/CI/badge.svg?branch=c_master)](https://github.com/msgpack/msgpack-c/actions) [![Build status](https://ci.appveyor.com/api/projects/status/8kstcgt79qj123mw/branch/c_master?svg=true)](https://ci.appveyor.com/project/redboltz/msgpack-c/branch/c_master)
[![codecov](https://codecov.io/gh/msgpack/msgpack-c/branch/c_master/graph/badge.svg)](https://codecov.io/gh/msgpack/msgpack-c/branch/c_master)

It's like JSON but smaller and faster.

Overview
--------

[MessagePack](http://msgpack.org/) is an efficient binary serialization
format, which lets you exchange data among multiple languages like JSON,
except that it's faster and smaller. Small integers are encoded into a
single byte and short strings require only one extra byte in
addition to the strings themselves.

Example
-------

```c
#include <msgpack.h>
#include <stdio.h>

int main(void)
{
    /* msgpack::sbuffer is a simple buffer implementation. */
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);

    /* serialize values into the buffer using msgpack_sbuffer_write callback function. */
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_array(&pk, 3);
    msgpack_pack_int(&pk, 1);
    msgpack_pack_true(&pk);
    msgpack_pack_str(&pk, 7);
    msgpack_pack_str_body(&pk, "example", 7);

    /* deserialize the buffer into msgpack_object instance. */
    /* deserialized object is valid during the msgpack_zone instance alive. */
    msgpack_zone mempool;
    msgpack_zone_init(&mempool, 2048);

    msgpack_object deserialized;
    msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);

    /* print the deserialized object. */
    msgpack_object_print(stdout, deserialized);
    puts("");

    msgpack_zone_destroy(&mempool);
    msgpack_sbuffer_destroy(&sbuf);

    return 0;
}
```

See [`QUICKSTART-C.md`](./QUICKSTART-C.md) for more details.

Usage
-----

### Building and Installing

#### Install from git repository

##### Using the Terminal (CLI)

You will need:

 - `gcc >= 4.1.0`
 - `cmake >= 2.8.0`

How to build:

    $ git clone https://github.com/msgpack/msgpack-c.git
    $ cd msgpack-c
    $ git checkout c_master
    $ cmake .
    $ make
    $ sudo make install

How to run tests:

In order to run tests you must have the [GoogleTest](https://github.com/google/googletest) framework installed. If you do not currently have it, install it and re-run `cmake`.
Then:

    $ make test

When you use the C part of `msgpack-c`, you need to build and link the library. By default, both static/shared libraries are built. If you want to build only static library, set `BUILD_SHARED_LIBS=OFF` to cmake. If you want to build only shared library, set `BUILD_SHARED_LIBS=ON`.

#### GUI on Windows

Clone msgpack-c git repository.

    $ git clone https://github.com/msgpack/msgpack-c.git

or using GUI git client.

e.g.) tortoise git https://code.google.com/p/tortoisegit/

1. Checkout to c_master branch

2. Launch [cmake GUI client](http://www.cmake.org/cmake/resources/software.html).

3. Set 'Where is the source code:' text box and 'Where to build
the binaries:' text box.

4. Click 'Configure' button.

5. Choose your Visual Studio version.

6. Click 'Generate' button.

7. Open the created msgpack.sln on Visual Studio.

8. Build all.

### Documentation

You can get additional information including the tutorial on the
[wiki](https://github.com/msgpack/msgpack-c/wiki).

Contributing
------------

`msgpack-c` is developed on GitHub at [msgpack/msgpack-c](https://github.com/msgpack/msgpack-c).
To report an issue or send a pull request, use the
[issue tracker](https://github.com/msgpack/msgpack-c/issues).

Here's the list of [great contributors](https://github.com/msgpack/msgpack-c/graphs/contributors).

License
-------

`msgpack-c` is licensed under the Boost Software License, Version 1.0. See
the [`LICENSE_1_0.txt`](./LICENSE_1_0.txt) file for details.
