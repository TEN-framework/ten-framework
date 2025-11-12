Debugging problems
==================

Check it's still a problem with latest lws
------------------------------------------

Older versions of lws don't attract any new work after they are released
(see [the release policy](https://libwebsockets.org/git/libwebsockets/tree/READMEs/README.release-policy.md) for details);
for a while they will get backported bugfixes but that's it.

All new work and bugfixes happen on `main` branch.

Old, old versions may be convenient for you to use for some reason.  But unless
you pay for support or have contributed work to lws so we feel we owe you some
consideration, nobody else has any reason to particularly care about solving
issues on ancient versions.  Whereas if the problem exists on `main`, and can be
reproduced by developers, it usually gets attention, often immediately.

If the problem doesn't exist on `main`, you can either use `main` or check also
the -stable branch of the last released version to see if it was already solved
there.

Library is a component
----------------------

As a library, lws is always just a component in a bigger application.

When users have a problem involving lws, what is happening in the bigger
application is usually critical to understand what is going on (and where the
solution lies).  Sometimes access to the remote peer like server or client is also
necessary to provoke the symptom.  Sometimes, the problem is in lws, but
sometimes the problem is not in lws but in these other pieces.

Many users are able to share their sources, but others decide not to, for
presumed "commercial advantage" or whatever.  (In any event, it can be painful
looking through large chunks of someone else's sources for problems when that
is not the library author's responsibility.)

This makes answering questions like "what is wrong with my code I am not
going to show you?" or even "what is wrong with my code?" very difficult.

Even if it's clear there is a problem somewhere, it cannot be understood or
reproduced by anyone else if it needs user code that isn't provided.

The biggest question is, "is this an lws problem actually"?  To solve that
the best solution is to strip out all or as much user code as possible,
and see if the problem is still coming.


Use the test apps / minimal examples as sanity checks
-----------------------------------------------------

The test server and client, and any more specifically relevant minimal example
 are extremely useful for sanity checks and debugging guidance.

 - **test apps work on your platform**, then either
   - your user code is broken, align it to how the test apps work, or,
   - something from your code is required to show an lws problem, provide a
     minimal patch on a test app so it can be reproduced
     
 - **test apps break on your platform**, but work on, eg, x86_64, either
   - toolchain or platform-specific (eg, OS) issue, or
   - lws platform support issue

 - **test apps break everywhere**
   - sounds like lws problem, info to reproduce and / or a patch is appreciated
