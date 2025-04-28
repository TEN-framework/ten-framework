# Tips about CMake

## Don't be afraid to nuke your build dir

CMake likes to cache options and other things in the build dir... if you stop
asserting the state of something like `-DMY_OPTION=1`, then the last way it was
set it cached.  On order to keep track of what you have set and not set, it's
very advisable to explicitly keep all your options and set them all on one cmake
line.

Then, when you meet a situation you changed something but somehow cmake is
sticking with what it knew before, you can fearlessly delete your build dir
and create a new one with your explicit config.

On Linux, it's usually enough to delete `CMakeCache.txt` to trigger it to config
from the start again, but on, eg, windows, it isn't, for whatever reason it
literally needs the build dir removing.

## CMake presence tests that fail

Lws makes use of various CMake features to figure out what apis your libraries
offer, eg, OpenSSL has many different apis based on version, lws knows how to
work around most of the changes, but to do it it must find out what apis are
available first on your build environment.

CMake basically builds little throwaway test programs using each api in turn, and
if it builds, it understands that the api was available and sets a preprocessor
symbol that's available in the main build accordingly.  Then we can do `#if xxx`
to figure out if we can use `xxx` or need to do a workaround at build-time.

This works very well, but unfortunately if the program didn't build, there are
many possible ways for the build to break even if the api being tested is
really available... for example, some library in your toolchain isn't being
linked for the throwaway test program.

When this happens, cmake indicates that apis that must be available are not available...
CMake keeps a log of what happened with the failed test programs in
`./build/CMakeFiles/CMakeError.log`.  This is appeneded to, so the best way is blow
away the build dir and reconfig a new one from scratch, and go look in there to
find out what the compiler or linker was complaining about.

