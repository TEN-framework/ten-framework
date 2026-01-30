# How to contribute

mruby is an open-source project which is looking forward to each contribution.
Contributors agree to license their contribution(s) under MIT license.

## Your Pull Request

To make it easy to review and understand your change please keep the following
things in mind before submitting your pull request:

- Work on the latest possible state of **mruby/master**
- Create a branch which is dedicated to your change
- Test your changes before creating a pull request (`rake test`)
- If possible write a test case which confirms your change
- Don't mix several features or bug-fixes in one pull request
- Create a meaningful commit message
- Explain your change (i.e. with a link to the issue you are fixing)
- Use mrbgem to provide non ISO features (classes, modules and methods) unless
  you have a special reason to implement them in the core

## pre-commit

A framework for managing and maintaining multi-language `pre-commit` hooks.
`pre-commit` can be [installed](https://pre-commit.com/#installation) with `pip`, `curl`, `brew` or `conda`.

You need to first install `pre-commit` and then install the `pre-commit` hooks with `pre-commit install`.
Now `pre-commit` will run automatically on git commit!

It's usually a good idea to run the hooks against all the files when adding new hooks (usually `pre-commit`
will only run on the changed files during git hooks). Use `pre-commit run --all-files` to check all files.

To run a single hook use `pre-commit run --all-files <hook_id>`

To update use `pre-commit autoupdate`

Sometimes you might need to skip one or more hooks which can be done with the `SKIP` environment variable.

`$ SKIP=yamllint git commit -m "foo"`

For convenience, we have added `pre-commit run --all-files`, `pre-commit install` and `pre-commit autoupdate`
to both the Makefile and the Rakefile. Run them with:

- `make check` or `rake check`
- `make checkinstall` or `rake checkinstall`
- `make checkupdate` or `rake checkupdate`

To configure `pre-commit` you can modify the config file [.pre-commit-config.yaml](.pre-commit-config.yaml).
We use [GitHub Actions](.github/workflows/lint.yml) to run `pre-commit` on every pull request.

### pre-commit quick links

- [Quick start](https://pre-commit.com/#quick-start)
- [Usage](https://pre-commit.com/#usage)
- [pre-commit autoupdate](https://pre-commit.com/#pre-commit-autoupdate)
- [Temporarily disabling hooks](https://pre-commit.com/#temporarily-disabling-hooks)

## Docker

We have both a `Dockerfile` and `docker-compose.yml` files in the repository root.
You can run these with the command line or use
[Docker Desktop](https://www.docker.com/products/docker-desktop/).

The Docker image is running Debian bullseye with Ruby and Python installed.
You can build the Docker image with:

`$ docker-compose build test`

So far we just have one service: `test`. Running the default `docker-compose`
command will create the Docker image, spin up a container and then build and
run all mruby tests.

The default `docker-compose` command is:

`$ docker-compose -p mruby run test`

You can also use Make or Rake to run the default `docker-compose`
command from above:

- `make composetest`
- `rake composetest`

List your Docker images with:

```console
$ docker images
REPOSITORY   TAG       IMAGE ID       CREATED          SIZE
mruby-test   latest    ec60f9536948   29 seconds ago   1.29GB
```

You can also run any custom `docker-compose` command which will override
the default. For example to run `pre-commit run --all-files` type:

`$ docker-compose -p mruby run test pre-commit run --all-files`

For convenience, you can also run `pre-commit` with:

- `make composecheck`
- `rake composecheck`

The bonus of running `pre-commit` with `docker-compose` is that you won't need
to install `pre-commit` and the hooks on your local machine. And that also
means you won't need to install `brew`, `conda` or `pip`.

Note limitation: currently running `pre-commit` with `docker-compose` we
skip the `check-executables-have-shebangs` hook.

Two more examples of custom `docker-compose` commands are:

- `$ docker-compose -p mruby run test ls`
- `$ docker-compose -p mruby run test rake doc:api`

If you want to test using a different `docker-compose` YAML config file you
can use the `-f` flag:

`$ docker-compose -p mruby -f docker-compose.test.yml run test`

- <https://docs.docker.com/compose/>
- <https://docs.docker.com/engine/reference/commandline/cli/>

## Spell Checking

We are using `pre-commit` to run [codespell](https://github.com/codespell-project/codespell)
to check code for common misspellings. We have a small custom dictionary file [codespell.txt](codespell.txt).

## Coding conventions

How to style your C and Ruby code which you want to submit.

### C code

The core part (parser, bytecode-interpreter, core-lib, etc.) of mruby is
written in the C programming language. Please note the following hints for your
C code:

#### Comply with C99 (ISO/IEC 9899:1999)

mruby should be highly portable to other systems and compilers. For this it is
recommended to keep your code as close as possible to the C99 standard
(<http://www.open-std.org/jtc1/sc22/WG14/www/docs/n1256.pdf>).

Visual C++ is also an important target for mruby (supported version is 2013 or
later). For this reason features that are not supported by Visual C++ may not
be used (e.g. `%z` of `strftime()`).

NOTE: Old GCC requires `-std=gnu99` option to enable C99 support.

#### Reduce library dependencies to a minimum

The dependencies to libraries should be kept to an absolute minimum. This
increases the portability but makes it also easier to cut away parts of mruby
on-demand.

#### Insert a break after the function return value:

```c
int
main(void)
{
  ...
}
```

### Ruby code

Parts of the standard library of mruby are written in the Ruby programming
language itself. Please note the following hints for your Ruby code:

#### Comply with the Ruby standard (ISO/IEC 30170:2012)

mruby is currently targeting to execute Ruby code which complies to ISO/IEC
30170:2012 (<https://www.iso.org/iso/iso_catalogue/catalogue_tc/catalogue_detail.htm?csnumber=59579>),
unless there's a clear reason, e.g. the latest Ruby has changed behavior from ISO.

## Building documentation

### mruby API

- [YARD](https://yardoc.org/) - YARD is a documentation generation tool for the Ruby programming language
- [yard-mruby](https://rubygems.org/gems/yard-mruby) - Document mruby sources with YARD
- [yard-coderay](https://rubygems.org/gems/yard-coderay) - Adds coderay syntax highlighting to YARD docs

### C API

- [Doxygen](https://www.doxygen.nl/) - Generate documentation from source code
- [Graphviz](https://graphviz.org/) - Graphviz is open source graph visualization software
