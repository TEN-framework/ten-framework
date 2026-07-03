# Guidance for porting to new platform

Where differences existed between the initial POSIX platform for lws and other
supported platforms like Windows, `lws_plat_...()` apis were added to move
handling to platform-specific code in `./lib/plat/`.

Depending o which platform is built, different platform-specific implementations
of these `lws_plat...()` apis get built.

## 1) Prepare the cmake cross-build file if necessary

CMake isolates its settings for cross-build into a separate file, which can be
used to different cmake projects for the same platform as well.

Find a similar examples already in `./contrib/cross-*` and copy and adapt it
as needed,

All settings related to toolchain should go in there.  For cross-toolchain,
the convention is to pass the path to its installed directory in `CROSS_PATH`
environment variable.

## 2) Copy the closest platform dir in ./lib/plat

Wholesale copy the closest existing platform dir to `/lib/plat/myplatform` and
rename the files.

Remove stuff specific to the original platform.

## 3) Add a flag in CMakeLists.txt

Cut and paste a flag to select your platform, preferably `LWS_PLAT_MYPLATFORM` or so

## 4) Add a section to force-select and deselect other cmake options based on platform flag

Some options on by default may not make sense on your platform, and others off
by default may be mandatory.  After the options() section in CMakeLists.txt, you
can use this kind of structure

```
	if (LWS_PLAT_MYPLATFORM)
		set(LWS_WITH_XXXX 0)
	endif()
```

to enforce implicit requirements of your platform.  Optional stuff should be set by
running cmake commandline as usual.

## 5) Add building your platform files into CMakeLists.txt

Add entries in CMakeLists.txt for building stuff in `./lib/plat/myplatform` when
`LWS_PLAT_MYPLATFORM` is enabled.

## 6) Adapt your copied ./lib/plat/myplatform/ files

You can now do test builds using the cross-build file, your platform flag in
cmake, and your copied ./lib/plat content... this last part since it was
copied from another platform will initially be a plentiful source of errors.

You can iteratively build and adapt the platform files.

