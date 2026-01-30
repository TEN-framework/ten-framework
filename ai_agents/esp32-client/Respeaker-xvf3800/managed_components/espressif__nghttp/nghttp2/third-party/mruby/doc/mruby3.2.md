# User visible changes in `mruby3.2` from `mruby3.1`

# The language

- Now `a::B = c` should evaluate `a` then `c`.
- Anonymous arguments `*`, `**`, `&` can be passed for forwarding.
- Multi-precision integer is available now via `mruby-bigint` gem.

# mruby VM and bytecode

- `OP_ARYDUP` was renamed to `OP_ARYSPLAT`. The instruction name
  was changed but instruction number and basic behavior have not
  changed (except that `ARYDUP nil` makes `[]`).

# Tools

## `mruby`

- `-b` only specifies the script is the binary. The files loaded by `-r` are not affected by the option.
- `mruby` now loads complied binary if the suffix is `.mrb`.

## `mrbc`

- Add `--no-optimize` option to disable optimization.

# mrbgems

## mruby-class-ext

- Add `Class#subclasses` method.
- Add `Module#undefined_instance_methods` method.

## New bundled gems

- mruby-errno from <https://github.com/iij/mruby-errno.git>
- mruby-set from <https://github.com/yui-knk/mruby-set.git>
- mruby-dir from <https://github.com/iij/mruby-dir.git>
- mruby-data

# Breaking Changes

## `mrb_vm_run()` may detach top-level local variables referenced from blocks

When the `mrb_vm_run()` function (including `mrb_top_run()`) is called,
the previous top-level local variables referenced from blocks is detached under either of the following conditions.

- If the `stack_keep` parameter is given as 0.
- If the number of variables in `irep` to be executed is less than the number of previous top-level local variables.

This change also affects API functions such as `mrb_load_string()` and `mrb_load_file()`.
The conditions under which the previous top-level local variables referenced from blocks is detached in these functions are as follows:

- If the function has no `mrbc_context` pointer parameter, or the `mrbc_context` pointer parameter is set to `NULL`.
- If the number of variables held in the `mrbc_context` pointer is less than the number of previous top-level local variables.

Intentional reliance on previous behavior may cause compatibility problems in your application.

# CVEs

Following CVEs are fixed.

- [CVE-2022-0080](https://www.cve.org/CVERecord?id=CVE-2022-0080)
- [CVE-2022-0240](https://www.cve.org/CVERecord?id=CVE-2022-0240)
- [CVE-2022-0326](https://www.cve.org/CVERecord?id=CVE-2022-0326)
- [CVE-2022-0481](https://www.cve.org/CVERecord?id=CVE-2022-0481)
- [CVE-2022-0525](https://www.cve.org/CVERecord?id=CVE-2022-0525)
- [CVE-2022-0570](https://www.cve.org/CVERecord?id=CVE-2022-0570)
- [CVE-2022-0614](https://www.cve.org/CVERecord?id=CVE-2022-0614)
- [CVE-2022-0623](https://www.cve.org/CVERecord?id=CVE-2022-0623)
- [CVE-2022-0630](https://www.cve.org/CVERecord?id=CVE-2022-0630)
- [CVE-2022-0631](https://www.cve.org/CVERecord?id=CVE-2022-0631)
- [CVE-2022-0632](https://www.cve.org/CVERecord?id=CVE-2022-0632)
- [CVE-2022-0717](https://www.cve.org/CVERecord?id=CVE-2022-0717)
- [CVE-2022-0890](https://www.cve.org/CVERecord?id=CVE-2022-0890)
- [CVE-2022-1106](https://www.cve.org/CVERecord?id=CVE-2022-1106)
- [CVE-2022-1212](https://www.cve.org/CVERecord?id=CVE-2022-1212)
- [CVE-2022-1276](https://www.cve.org/CVERecord?id=CVE-2022-1276)
- [CVE-2022-1286](https://www.cve.org/CVERecord?id=CVE-2022-1286)
- [CVE-2022-1934](https://www.cve.org/CVERecord?id=CVE-2022-1934)
