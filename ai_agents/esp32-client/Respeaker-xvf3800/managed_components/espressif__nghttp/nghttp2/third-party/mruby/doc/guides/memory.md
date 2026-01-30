# Memory Allocation

There are three methods to customize memory allocation in mruby.

1. Provide your own `realloc()`/`free()`
2. Redefine `mrb_default_allocf()`
3. Specify a function with `mrb_open_allocf()`

## Provide your own `realloc()`/`free()`

On some platforms, especially on microcontrollers, the standard library may not provide `malloc()`, `realloc()`, and `free()`. In such cases, it may be necessary to define memory allocation functions for the specific platform. mruby uses `realloc()` and `free()` from the standard C library for memory management. By defining these two functions of your own, you can make mruby work. However, note the following two points:

First, `realloc(NULL, size)` behaves the same as malloc(size). Second, `free(NULL)` exits without doing anything.

## Redefine `mrb_default_allocf()`

The only function in mruby that uses the standard C library's memory allocation functions is `mrb_default_allocf()`, defined in `alloc.c`. By defining this function within your application, you can customize the memory management of your application.

## Specify a function with `mrb_open_allocf()`

If you want to perform different memory management for each `mrb_state` within your application, you can use the `mrb_open_allocf()` function to create the `mrb_state` structure. This allows you to specify a memory allocation function (which is compatible with `mrb_default_allocf`) for each `mrb_state`. Although this scheme is not recommended. It may become obsolete in the future, since I have never seen per mrb_state memory management use-case.
