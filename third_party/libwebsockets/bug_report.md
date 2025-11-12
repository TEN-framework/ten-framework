** What version of lws **

"vx.y.z" or "01234567 from `main` thismorning" etc

If it's much older than last stable release, we will likely suggest you try that
or `main`.

** What platform and arch? **

"Fedora 32 x86_64" or "OSX Catalina" etc

** What parts of lws does it involve? **

dunno / core / client / server
raw / http / ws / mqtt / other (give me a hint)

** How can I reproduce the problem just using lws code? **

We can't guess your problem especially in your code.  It's great if you can give us a way to
realize our own failure clearly with a reproducer that uses our own code.

Try to remove your code from the equation by trying the same flow on an lws minimal example and provide a little diff against that. We can find out if it's only on your platform, or only on that version, or only in your code from that quickly, and if something to fix in lws, I can confirm it really is fixed using the same test.

** Describe the bug **

    "fails" --> this word is a red flag you didn't try to debug the issue much... exactly how does it "fail", what evidence is it leaving like logs or return codes or traces?
    "hangs" --> this word is a red flag you didn't try to debug the issue much... exactly what does it mean, whole device frozen? Spinning 100% cpu? Just idle? Building on fire? Have you tried it via strace or similar if it seems frozen to see what it's doing? Attach a debugger like gdb -p pid and get a backtrace? perf top if Linux to see what it spends its time on.
    "crashes" --> what happens if you run under valgrind? You know lws is not threadsafe except for lws_cancel_service(), right...
    "sucks" --> let's discuss you writing a patch to improve whatever it is

** Additional data **

Build problems? Describe the toolchain and paste the warnings / errors.

Crash? Get a usable backtrace by building with `cmake .. -DCMAKE_BUILD_TYPE=DEBUG` and run under gdb, lldb, or valgrind.

Mysterious happenings? Get verbose lws logs by building with `cmake .. -DCMAKE_BUILD_TYPE=DEBUG` and run with `lws_set_log_level(1151, NULL)`, on the example apps they all take a switch like -d1151.

