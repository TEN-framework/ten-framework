# Security Process

If you find a vulnerability in our software, please report it via
GitHub "Private vulnerability reporting" feature at
https://github.com/nghttp2/nghttp2/security instead of submitting
issues on github issue page.  It is a standard practice not to
disclose vulnerability information publicly until a fixed version is
released, or mitigation is worked out.

If we identify that the reported issue is really a vulnerability, we
open a new security advisory draft using [GitHub security
feature](https://github.com/nghttp2/nghttp2/security) and discuss the
mitigation and bug fixes there.  The fixes are committed to the
private repository.

We write the security advisory and get CVE number from GitHub
privately.  We also discuss the disclosure date to the public.

We make a new release with the fix at the same time when the
vulnerability is disclosed to public.

At least 7 days before the public disclosure date, we open a new issue
on [nghttp2 issue tracker](https://github.com/nghttp2/nghttp2/issues)
which notifies that the upcoming release will have a security fix.
The `SECURITY` label is attached to this kind of issue.  The issue is
not opened if a vulnerability is already disclosed, and it is publicly
known that nghttp2 is affected by that.

Before few hours of new release, we merge the fixes to the master
branch (and/or a release branch if necessary) and make a new release.
Security advisory is disclosed on GitHub.
