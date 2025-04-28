# Some notes for the windows jungle

This was how I compiled libwebsockets starting from a blank windows install
in March - April 2020.  Doing this on a linux distro is way simpler and quicker
than all this!

## Notes on vm installation

### Disk size

For building you'll need 40GB+ available for the guest storage.

### Required: Windows product key

Assuming like me the first thing you do with a new laptop is install Linux over
the windows it came with, you can recover your 'windows tax' windows product key
from your device typically using `sudo strings /sys/firmware/acpi/tables/MSDM`,
and use that for your VM install.

### Required: Spice guest 

To have shared clipboard, and for windows video driver to match your vm window
resolution, you must install spice guest tools inside the windows VM.  It also
installs some virtio pieces you will want.

https://www.spice-space.org/download/windows/spice-guest-tools/spice-guest-tools-latest.exe

### Blood-pressure reduction: Firefox

https://www.mozilla.org/en-US/exp/firefox/

When it's up, add-ons: ublock origin, privacy badger, noscript, disable search
bar prediction

### Blood-pressure reduction: Clink

This is a hack on cmd.exe that lets it understand Ctrl-R and fixup unix-style
slashes automagically.

https://github.com/mridgers/clink/releases/download/0.4.9/clink_0.4.9_setup.exe

If you're usually using *nix, you definitely need this to keep your sanity.

### Required: cmake

CMake have a windows installer thing downloadable from here

[cmake](https://cmake.org/download/)

after that you can use `cmake` from the terminal OK.

### Required: git

Visit the canonical git site to download their windows installer thing

[git](https://git-scm.com/download/win)

**Select the install option for "extra unix commands"** so you can get `ls -l`,
`cp`, `mv` and suchlike working in cmd.exe... that's awesome, thanks git!

Afterwards you can just use `git` as normal from cmd.exe as well.

### Required: Install the "free" "community" visual studio

You can do this through "windows store" by searching for "visual studio"

I installed as little as possible, we just want the C "C++" tools... 7GB :-)

It still wouldn't link without the "mt" helper tool from the
huge windows SDK, so you have to install GB of that as well.

They don't mention it during the install, but after 30 days this "free"
"community" edition demands you open a microsoft account or it stops working.
In the install they give you the option to add a microsoft account and the
alternative is, "not now, maybe later".  Compare and contrast to gcc or git or
the other FOSS projects.

### Required: OpenSSL

Ugh... I tried using prebuilts but it's unreliable and needs an unfeasible
amount of trust.  So I recommend bite the bullet and build your own... that's
trivial on Linux but of course windows makes everything nasty.

At least hopefully all the "research" is done and listed out here.

#### OpenSSL build Prerequisite: install perl binary

Move the git version of perl out of the way, it won't work for OpenSSL build

```
mv /usr/bin/perl /usr/bin/perl-git
```

For windows, OpenSSL "recommends" ActiveState perl but it doesn't work for me,
complaining about stuff needed from cpan and then dying when it was installed.
"Strawberry Perl" is installed in `C:\Strawberry` and worked out the box.

http://strawberryperl.com/download/5.30.2.1/strawberry-perl-5.30.2.1-64bit.msi

The installer sets up `%PATH%` if you open a new cmd window.  

#### OpenSSL build Prerequisite: NASM

Go here and click on the latest stable, download the win32 .exe

https://nasm.us/

Just install via the defaults.  Then add it to the PATH temporarily...

```
$ set PATH=%PATH%;C:\Program Files (x86)\NASM
```

#### OpenSSL build setup: source VC env vars

These fix up the PATH and include dirs etc necessary for VC build in the cmd
window.

```
$ call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
```

### OpenSSL build:

Grab openssl from git... assuming the prerequisites above went well it will
just sit there building for 30 minutes or whatever.

```
$ git clone https://github.com/openssl/openssl
$ cd openssl
$ perl Configure VC-WIN64A
$ nmake
```

Afterwards, open an Administrator mode cmd.exe, redo the msvc path and then
install the build.

```
$ cd openssl
$ call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
$ nmake install
```

Oh another grindingly slow windows build action.  Finally it's in there in
`C:\Program Files\OpenSSL`.

libraries are looking for a cert bundle at "C:\Program Files\Common Files\SSL\cert.pem"...
it's not documented or included in the zip file from the above, so...

#### Installing a cert bundle

You can get a trusted cert bundle from here

[drwetter/testssl cert bundle](https://raw.githubusercontent.com/drwetter/testssl.sh/3.1dev/etc/Microsoft.pem)

Save it into `C:\Program Files\Common Files\SSL\cert.pem` where openssl will be able to see it.

## Required: pthreads

It's amazing but after all these years windows doesn't offer pthreads compatibility
itself.  Just like the many other missing POSIX bits like fork().

I downloaded the latest (2012) zip release of pthreads-win32 from here

ftp://sourceware.org/pub/pthreads-win32

Then I created a dir "C:\Program Files (x86)\pthreads", and copied the `dll`,
`include` and `lib` subdirs from the `prebuilt` folder in the zip there.

The cmake incantation to build against pthreads set up like that is

```
 $ cmake .. -DLWS_HAVE_PTHREAD_H=1 -DLWS_EXT_PTHREAD_INCLUDE_DIR="C:\Program Files (x86)\pthreads\include" -DLWS_EXT_PTHREAD_LIBRARIES="C:\Program Files (x86)\pthreads\lib\x64\libpthreadGC2.a" -DLWS_WITH_MINIMAL_EXAMPLES=1
```

## Building libwebsockets

We'll clone libwebsockets then use cmake to build via vs tools

```
> git clone https://libwebsockets.org/repo/libwebsockets
> cd libwebsockets
> mkdir build
> cd build
> cmake ..
> cmake --build . --config DEBUG
```

Installing requires admin privs, I opened a second cmd window as admin and did it
there.

```
> cmake --install . --config DEBUG
```

### Hack the libs into view

The libs we built against aren't visible in the system, I don't know what
Real Windows Programmers are supposed to do about that, but I used an Admin cmd
prompt to copy them into C:\windows\system32

```
$ cp "C:\Program Files (x86)\pthreads\dll\x64\pthreadGC2.dll" "C:\Program Files\OpenSSL\bin\libcrypto-3.dll"  "C:\Program Files\OpenSSL\bin\libssl-3.dll" C:\Windows\system32
```

After that you can run the test apps OK, eg

```
$ libwebsockets-test-server.exe -s
```

## Note about using paths with spaces in with cmake


