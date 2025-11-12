## Using CTest with lws

### Updating ancient cmake

You need a recent cmake to have the CTest tests work properly, if you're on an
older distro you need to update your cmake.  Luckily Kitware provide a repo for
common distros.  These instructions work for bionic and xenial.

First remove the old distro cmake and install the pieces needed to get the new repo keys

```
# apt purge --auto-remove cmake
# apt install gnupg wget apt-transport-https ca-certificates
# wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | sudo apt-key add -
# apt edit-sources
```

Add the line `deb https://apt.kitware.com/ubuntu/ bionic main` at the end
replacing `bionic` with `xenial` as needed, and save (:wq).  Then

```
# apt update
# apt install cmake
```

## Tests live in CMakeLists.txt

The rules for tests are described in ctest / cmake language inside the minimal
examples and api tests that are enabled by current build options, so you need
to build with `-DLWS_WITH_MINIMAL_EXAMPLES=1` to build the examples along with
the library.

The tests are typically running the examples or api tests and regarding the
process exiting with exit code 0 as success, anything else as failure.

## Generating the tests

The main tests just need `-DLWS_WITH_MINIMAL_EXAMPLES=1`.  You can optionally set
`-DLWS_CTEST_INTERNET_AVAILABLE=0` to indicate you can't run the tests that need
internet connectivity.

## Preparing to run the tests

The tests have to be able to run without root and without disturbing any other
install of lws in the build machine.

For that reason you have to do an unprivileged side-install into `../destdir`,
using `make install DESTDIR=../destdir` from the build directory and perform the
tests on the pieces in there.

## Running the tests

We must take care to run the pieces (.so etc) we just built, without having
root access, and not any of the same pieces from some other lws version that may
have been installed on the build machine.  That includes, eg, plugins that
we just built, to ensure precedence of those in the search path we can set our
DESTDIR unprivileged install path in `LD_LIBRARY_PATH`.

Then we can run ctest on the unprivileged install.  The whole step looks
something like this:

```
build $ make -j12 && \
  rm -rf ../destdir && \
  make -j12 DESTDIR=../destdir install && \\
  LD_LIBRARY_PATH=../destdir/usr/local/share/libwebsockets-test-server/plugins ctest -j2 --output-on-failure
```  

On windows, it looks like `ctest . -C DEBUG` or RELEASE if that was the build
type.

Good results look something like this (which tests can run depend on your
build options)

```
Test project /projects/libwebsockets/build
      Start 71: st_wcs_srv
      Start 43: st_hcp_srv
 1/73 Test #71: st_wcs_srv ..................................   Passed    5.01 sec
      Start 19: st_hcmp_srv
 2/73 Test #43: st_hcp_srv ..................................   Passed    5.01 sec
      Start 17: st_hcm_srv
 3/73 Test #19: st_hcmp_srv .................................   Passed    5.01 sec
      Start 55: st_ssproxyctx
 4/73 Test #17: st_hcm_srv ..................................   Passed    5.01 sec
      Start 52: st_ssproxy
 5/73 Test #55: st_ssproxyctx ...............................   Passed    1.02 sec
      Start 67: st_sstfproxy
 6/73 Test #52: st_ssproxy ..................................   Passed    1.02 sec
      Start 60: st_ssprxsmd_sspc
 7/73 Test #67: st_sstfproxy ................................   Passed    1.01 sec
      Start 63: st_mulssprxsmd_sspc
 8/73 Test #60: st_ssprxsmd_sspc ............................   Passed    1.01 sec
      Start 69: sspc-minimaltf
 9/73 Test #63: st_mulssprxsmd_sspc .........................   Passed    1.02 sec
      Start 73: ws-client-spam
10/73 Test #73: ws-client-spam ..............................   Passed   12.21 sec
      Start 57: sspc-minimaltx
11/73 Test #57: sspc-minimaltx ..............................   Passed    5.90 sec
      Start 65: mulsspcsmd_sspc
12/73 Test #65: mulsspcsmd_sspc .............................   Passed    3.58 sec
      Start 62: sspcsmd_sspc
13/73 Test #62: sspcsmd_sspc ................................   Passed    1.73 sec
      Start 22: http-client-multi-h1
14/73 Test #22: http-client-multi-h1 ........................   Passed    5.04 sec
      Start 25: http-client-multi-stag
15/73 Test #25: http-client-multi-stag ......................   Passed    4.53 sec
      Start 26: http-client-multi-stag-h1
16/73 Test #26: http-client-multi-stag-h1 ...................   Passed    4.40 sec
      Start 21: http-client-multi
17/73 Test #21: http-client-multi ...........................   Passed    4.37 sec
      Start 36: http-client-multi-post-h1
18/73 Test #36: http-client-multi-post-h1 ...................   Passed    2.73 sec
      Start 54: sspc-minimal
19/73 Test #54: sspc-minimal ................................   Passed    0.93 sec
      Start 39: http-client-multi-post-stag
20/73 Test #39: http-client-multi-post-stag .................   Passed    2.29 sec
      Start 40: http-client-multi-post-stag-h1
21/73 Test #69: sspc-minimaltf ..............................   Passed   49.83 sec
      Start 35: http-client-multi-post
22/73 Test #40: http-client-multi-post-stag-h1 ..............   Passed    4.30 sec
      Start 33: http-client-multi-restrict-nopipe-fail
23/73 Test #35: http-client-multi-post ......................   Passed    3.23 sec
      Start 28: http-client-multi-stag-h1-pipe
24/73 Test #33: http-client-multi-restrict-nopipe-fail ......   Passed    2.86 sec
      Start 32: http-client-multi-restrict-stag-h1-pipe
25/73 Test #28: http-client-multi-stag-h1-pipe ..............   Passed    2.86 sec
      Start 27: http-client-multi-stag-pipe
26/73 Test #32: http-client-multi-restrict-stag-h1-pipe .....   Passed    1.51 sec
      Start 31: http-client-multi-restrict-stag-pipe
27/73 Test #27: http-client-multi-stag-pipe .................   Passed    1.52 sec
      Start 34: http-client-multi-restrict-h1-nopipe-fail
28/73 Test #34: http-client-multi-restrict-h1-nopipe-fail ...   Passed    2.78 sec
      Start 46: http-client-post-m
29/73 Test #31: http-client-multi-restrict-stag-pipe ........   Passed    2.80 sec
      Start 42: http-client-multi-post-stag-h1-pipe
30/73 Test #42: http-client-multi-post-stag-h1-pipe .........   Passed    1.51 sec
      Start 41: http-client-multi-post-stag-pipe
31/73 Test #46: http-client-post-m ..........................   Passed    1.59 sec
      Start 48: http-client-post-m-h1
32/73 Test #48: http-client-post-m-h1 .......................   Passed    1.10 sec
      Start 23: http-client-multi-pipe
33/73 Test #41: http-client-multi-post-stag-pipe ............   Passed    1.51 sec
      Start 29: http-client-multi-restrict-pipe
34/73 Test #23: http-client-multi-pipe ......................   Passed    1.09 sec
      Start 24: http-client-multi-h1-pipe
35/73 Test #29: http-client-multi-restrict-pipe .............   Passed    0.74 sec
      Start 30: http-client-multi-restrict-h1-pipe
36/73 Test #24: http-client-multi-h1-pipe ...................   Passed    1.14 sec
      Start 45: http-client-post
37/73 Test #30: http-client-multi-restrict-h1-pipe ..........   Passed    1.14 sec
      Start 38: http-client-multi-post-h1-pipe
38/73 Test #45: http-client-post ............................   Passed    0.30 sec
      Start 37: http-client-multi-post-pipe
39/73 Test #38: http-client-multi-post-h1-pipe ..............   Passed    0.49 sec
      Start 47: http-client-post-h1
40/73 Test #37: http-client-multi-post-pipe .................   Passed    0.31 sec
      Start 50: hs_evlib_foreign_event
41/73 Test #47: http-client-post-h1 .........................   Passed    0.29 sec
      Start 66: ss-tf
42/73 Test #50: hs_evlib_foreign_event ......................   Passed   22.02 sec
      Start 49: hs_evlib_foreign_uv
43/73 Test #49: hs_evlib_foreign_uv .........................   Passed   21.03 sec
      Start 51: ss-warmcat
44/73 Test #51: ss-warmcat ..................................   Passed    2.69 sec
      Start 59: ss-smd
45/73 Test #59: ss-smd ......................................   Passed    1.78 sec
      Start 10: api-test-secure-streams
46/73 Test #10: api-test-secure-streams .....................   Passed    1.34 sec
      Start 11: http-client-warmcat
47/73 Test #11: http-client-warmcat .........................   Passed    0.27 sec
      Start 58: sspost-warmcat
48/73 Test #58: sspost-warmcat ..............................   Passed    0.84 sec
      Start 12: http-client-warmcat-h1
49/73 Test #12: http-client-warmcat-h1 ......................   Passed    0.25 sec
      Start  2: api-test-jose
50/73 Test  #2: api-test-jose ...............................   Passed    0.27 sec
      Start 70: ws-client-rx-warmcat
51/73 Test #70: ws-client-rx-warmcat ........................   Passed    0.27 sec
      Start 56: ki_ssproxyctx
52/73 Test #56: ki_ssproxyctx ...............................   Passed    0.12 sec
      Start 68: ki_ssproxy
53/73 Test #68: ki_ssproxy ..................................   Passed    0.11 sec
      Start 64: ki_mulssprxsmd_sspc
54/73 Test #64: ki_mulssprxsmd_sspc .........................   Passed    0.10 sec
      Start 61: ki_ssprxsmd_sspc
55/73 Test #61: ki_ssprxsmd_sspc ............................   Passed    0.11 sec
      Start 13: http-client-h2-rxflow-warmcat
56/73 Test #13: http-client-h2-rxflow-warmcat ...............   Passed    0.28 sec
      Start 14: http-client-h2-rxflow-warmcat-h1
57/73 Test #14: http-client-h2-rxflow-warmcat-h1 ............   Passed    0.34 sec
      Start 16: http-client-hugeurl-warmcat-h1
58/73 Test #16: http-client-hugeurl-warmcat-h1 ..............   Passed    0.16 sec
      Start 15: http-client-hugeurl-warmcat
59/73 Test #15: http-client-hugeurl-warmcat .................   Passed    0.16 sec
      Start 72: ki_wcs_srv
60/73 Test #72: ki_wcs_srv ..................................   Passed    0.12 sec
      Start 44: ki_hcp_srv
61/73 Test #44: ki_hcp_srv ..................................   Passed    0.11 sec
      Start 20: ki_hcmp_srv
62/73 Test #20: ki_hcmp_srv .................................   Passed    0.11 sec
      Start 18: ki_hcm_srv
63/73 Test #18: ki_hcm_srv ..................................   Passed    0.11 sec
      Start  7: api-test-lws_struct_sqlite
64/73 Test  #7: api-test-lws_struct_sqlite ..................   Passed    0.03 sec
      Start  1: api-test-gencrypto
65/73 Test  #1: api-test-gencrypto ..........................   Passed    0.02 sec
      Start  6: api-test-lws_struct-json
66/73 Test  #6: api-test-lws_struct-json ....................   Passed    0.01 sec
      Start  4: api-test-lws_dsh
67/73 Test  #4: api-test-lws_dsh ............................   Passed    0.01 sec
      Start  8: api-test-lws_tokenize
68/73 Test  #8: api-test-lws_tokenize .......................   Passed    0.01 sec
      Start  9: api-test-lwsac
69/73 Test  #9: api-test-lwsac ..............................   Passed    0.00 sec
      Start  3: api-test-lejp
70/73 Test  #3: api-test-lejp ...............................   Passed    0.00 sec
      Start 53: ki_ssproxy
71/73 Test #53: ki_ssproxy ..................................   Passed    0.11 sec
72/73 Test #66: ss-tf .......................................   Passed   55.51 sec
      Start  5: api-test-lws_smd
73/73 Test  #5: api-test-lws_smd ............................   Passed    4.22 sec

100% tests passed, 0 tests failed out of 73

Total Test time (real) = 137.76 sec
```

## Considerations for creating tests

### Timeout

The default test timeout is 1500s, for that reason it's good practice to set
a more suitable `TIMEOUT` property on every test.

### Working Directory

Server-side test apps usually need to be run from their `./minimal-examples/...`
directory so they can access their assets like index.html etc.

However when building with `-DLWS_WITH_MBEDTLS=1` then even client-side apps
need to be run from their directory, since they need to get the trusted CA for
warmcat.com or libwebsockets.org additionally.

For that reason it's good practice to set the `WORKING_DIRECTORY` property to
the home dir of the example app in all cases.

### Spawning Buddies

Many networking tests need to either spawn a client or a server in order to
have a "buddy" to talk to during the test for the opposing side.  This is a
bit awkward in cmake since it does not directly support spawning daemons as
test dependencies.

Lws provides helper scripts for unix type targets in `./scripts/ctest-background.sh`
and `./scripts/ctest-background-kill.sh`, which spawn background processes,
save the pid in a decorated /tmp file and can later take the process down.  This
also has arrangements to dump the log of any background process that exited
early.

To arrange the buddy to run aligned with the test, you first explain to cmake
how to start and stop the buddy using phony tests to make a "fixture" in cmake
terms.

In this example, taken from minimal-http-client-multi, we arrange for
minimal-http-server-tls to be available for our actual test.  The starting and
stopping definition, for "st_hcm_srv" and "ki_hcm_srv":

```
	add_test(NAME st_hcm_srv COMMAND
		${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
			hcm_srv $<TARGET_FILE:lws-minimal-http-server-tls>
			--port ${PORT_HCM_SRV} )
	add_test(NAME ki_hcm_srv COMMAND
		${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh
			hcm_srv $<TARGET_FILE_NAME:lws-minimal-http-server-tls>
				--port ${PORT_HCM_SRV})
```

... and binding those together so cmake knows they start and stop a specific
named fixture "hcm_srv", itself with an 800s timeout

```
	set_tests_properties(st_hcm_srv PROPERTIES
       		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/http-server/minimal-http-server-tls
		FIXTURES_SETUP hcm_srv
		TIMEOUT 800)
	set_tests_properties(ki_hcm_srv PROPERTIES
		FIXTURES_CLEANUP hcm_srv)
```

... and finally, adding the "hcm_srv" fixture as a requirement on the actual
test (http-client-multi) we are testing

```
	set_tests_properties(http-client-multi
			     PROPERTIES
			     FIXTURES_REQUIRED "hcm_srv"
			     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/http-client/minimal-http-client-multi
			     TIMEOUT 50)
```

Once all that explaining is done, ctest itself will take care about starting
and killing hcm_srv before and after http-client-multi test.

### Buddy sockets and test concurrency

For tests with local buddies using tcp sockets inside the same VM or systemd-
nspawn networking context, you cannot just use a well-known port like 7681.

ctest itself is usually executed concurrently, and Sai is typically building
multiple different instances concurrently as well (typically 3), so it may be
running different ctests inside the same VM simultaneously.

Different tests can have their own convention for port ranges, to solve the
problem about Sai running different tests concurrently inside one ctest.

For the case there are multiple ctests running, we can use the env var
`$ENV{SAI_INSTANCE_IDX}`, which is an ordinal like 0 or 1, to further ensure
that port selections won't conflict.  If not using Sai, you can just set this
in the evironment yourself to reflect your build instance index.

```
       #
       # instantiate the server per sai builder instance, they are running in the same
       # machine context in parallel so they can tread on each other otherwise
       #
       set(PORT_HCM_SRV "7670")
       if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "0")
               set(PORT_HCM_SRV 7671)
       endif()
       if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "1")
               set(PORT_HCM_SRV 7672)
       endif()
       if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "2")
               set(PORT_HCM_SRV 7673)
       endif()
       if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "3")
               set(PORT_HCM_SRV 7674)
       endif()
```

This is complicated enough that the best approach is copy an existing simple
case like the CMakeLists.txt for minimal-http-client and change the names and
ports to be unique.
