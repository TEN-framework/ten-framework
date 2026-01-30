# nghttp2 - HTTP/2 C Library

# Copyright (c) 2014 Tatsuhiro Tsujikawa

# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:

# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

AM_CPPFLAGS = @DEFS@

EXTRA_DIST = CMakeLists.txt build_config.rb

# Enumerate all mruby files with the following command:
# find mruby -type f ! -ipath 'mruby/.*' | awk '{print "\t"$0" \\"}'
EXTRA_DIST += \
	mruby/AUTHORS \
	mruby/docker-compose.yml \
	mruby/codespell.txt \
	mruby/CONTRIBUTING.md \
	mruby/NEWS \
	mruby/CODEOWNERS \
	mruby/Gemfile.lock \
	mruby/appveyor.yml \
	mruby/Dockerfile \
	mruby/Gemfile \
	mruby/benchmark/plot.gpl \
	mruby/benchmark/bm_ao_render.rb \
	mruby/benchmark/bm_fib.rb \
	mruby/benchmark/bm_so_lists.rb \
	mruby/benchmark/bm_app_lc_fizzbuzz.rb \
	mruby/Makefile \
	mruby/LEGAL \
	mruby/tasks/libmruby.rake \
	mruby/tasks/core.rake \
	mruby/tasks/toolchains/visualcpp.rake \
	mruby/tasks/toolchains/openwrt.rake \
	mruby/tasks/toolchains/android.rake \
	mruby/tasks/toolchains/gcc.rake \
	mruby/tasks/toolchains/clang.rake \
	mruby/tasks/doc.rake \
	mruby/tasks/benchmark.rake \
	mruby/tasks/mrbgems.rake \
	mruby/tasks/presym.rake \
	mruby/tasks/install.rake \
	mruby/tasks/bin.rake \
	mruby/tasks/test.rake \
	mruby/tasks/mrblib.rake \
	mruby/README.md \
	mruby/oss-fuzz/ruby.proto \
	mruby/oss-fuzz/proto_to_ruby.cpp \
	mruby/oss-fuzz/mruby_proto_fuzzer.cpp \
	mruby/oss-fuzz/config/mruby.dict \
	mruby/oss-fuzz/config/mruby_proto_fuzzer.options \
	mruby/oss-fuzz/config/mruby_fuzzer.options \
	mruby/oss-fuzz/mruby_fuzzer.c \
	mruby/oss-fuzz/proto_to_ruby.h \
	mruby/build_config/cross-mingw-winetest.rb \
	mruby/build_config/host-f32.rb \
	mruby/build_config/nintendo_wii.rb \
	mruby/build_config/i586-pc-msdosdjgpp.rb \
	mruby/build_config/chipKITMax32.rb \
	mruby/build_config/gameboyadvance.rb \
	mruby/build_config/minimal.rb \
	mruby/build_config/host-m32.rb \
	mruby/build_config/host-shared.rb \
	mruby/build_config/host-debug.rb \
	mruby/build_config/bench.rb \
	mruby/build_config/host-cxx.rb \
	mruby/build_config/mrbc.rb \
	mruby/build_config/ArduinoDue.rb \
	mruby/build_config/IntelEdison.rb \
	mruby/build_config/boxing.rb \
	mruby/build_config/IntelGalileo.rb \
	mruby/build_config/ci/msvc.rb \
	mruby/build_config/ci/gcc-clang.rb \
	mruby/build_config/nintendo_switch.rb \
	mruby/build_config/serenity.rb \
	mruby/build_config/dreamcast_shelf.rb \
	mruby/build_config/default.rb \
	mruby/build_config/android_armeabi_v7a_neon_hard.rb \
	mruby/build_config/host-gprof.rb \
	mruby/build_config/clang-asan.rb \
	mruby/build_config/RX630.rb \
	mruby/build_config/host-nofloat.rb \
	mruby/build_config/android_arm64_v8a.rb \
	mruby/build_config/cross-32bit.rb \
	mruby/build_config/cross-mingw.rb \
	mruby/build_config/helpers/wine_runner.rb \
	mruby/lib/mruby/build/command.rb \
	mruby/lib/mruby/build/load_gems.rb \
	mruby/lib/mruby/lockfile.rb \
	mruby/lib/mruby/build.rb \
	mruby/lib/mruby/core_ext.rb \
	mruby/lib/mruby/doc.rb \
	mruby/lib/mruby/gem.rb \
	mruby/lib/mruby/presym.rb \
	mruby/lib/mruby/source.rb \
	mruby/examples/mrbgems/ruby_extension_example/mrbgem.rake \
	mruby/examples/mrbgems/ruby_extension_example/README.md \
	mruby/examples/mrbgems/ruby_extension_example/test/example.rb \
	mruby/examples/mrbgems/ruby_extension_example/mrblib/example.rb \
	mruby/examples/mrbgems/cdata_extension_example/mrbgem.rake \
	mruby/examples/mrbgems/cdata_extension_example/README.md \
	mruby/examples/mrbgems/cdata_extension_example/test/example.c \
	mruby/examples/mrbgems/cdata_extension_example/src/example.c \
	mruby/examples/mrbgems/c_and_ruby_extension_example/mrbgem.rake \
	mruby/examples/mrbgems/c_and_ruby_extension_example/README.md \
	mruby/examples/mrbgems/c_and_ruby_extension_example/test/example.rb \
	mruby/examples/mrbgems/c_and_ruby_extension_example/mrblib/example.rb \
	mruby/examples/mrbgems/c_and_ruby_extension_example/src/example.c \
	mruby/examples/mrbgems/c_extension_example/mrbgem.rake \
	mruby/examples/mrbgems/c_extension_example/README.md \
	mruby/examples/mrbgems/c_extension_example/test/example.rb \
	mruby/examples/mrbgems/c_extension_example/test/example.c \
	mruby/examples/mrbgems/c_extension_example/src/example.c \
	mruby/examples/mrbgems/mruby-YOUR-bigint/TODO-HINT.md \
	mruby/examples/mrbgems/mruby-YOUR-bigint/core/bigint.c \
	mruby/examples/mrbgems/mruby-YOUR-bigint/mrbgem.rake \
	mruby/test/bintest.rb \
	mruby/test/t/enumerable.rb \
	mruby/test/t/comparable.rb \
	mruby/test/t/false.rb \
	mruby/test/t/exception.rb \
	mruby/test/t/rangeerror.rb \
	mruby/test/t/bs_literal.rb \
	mruby/test/t/regexperror.rb \
	mruby/test/t/class.rb \
	mruby/test/t/iterations.rb \
	mruby/test/t/true.rb \
	mruby/test/t/lang.rb \
	mruby/test/t/range.rb \
	mruby/test/t/kernel.rb \
	mruby/test/t/unicode.rb \
	mruby/test/t/localjumperror.rb \
	mruby/test/t/hash.rb \
	mruby/test/t/syntax.rb \
	mruby/test/t/nameerror.rb \
	mruby/test/t/module.rb \
	mruby/test/t/argumenterror.rb \
	mruby/test/t/methods.rb \
	mruby/test/t/ensure.rb \
	mruby/test/t/indexerror.rb \
	mruby/test/t/runtimeerror.rb \
	mruby/test/t/gc.rb \
	mruby/test/t/array.rb \
	mruby/test/t/string.rb \
	mruby/test/t/proc.rb \
	mruby/test/t/basicobject.rb \
	mruby/test/t/integer.rb \
	mruby/test/t/nomethoderror.rb \
	mruby/test/t/codegen.rb \
	mruby/test/t/literals.rb \
	mruby/test/t/nil.rb \
	mruby/test/t/typeerror.rb \
	mruby/test/t/symbol.rb \
	mruby/test/t/object.rb \
	mruby/test/t/vformat.rb \
	mruby/test/t/numeric.rb \
	mruby/test/t/bs_block.rb \
	mruby/test/t/float.rb \
	mruby/test/t/standarderror.rb \
	mruby/test/t/superclass.rb \
	mruby/test/assert.rb \
	mruby/TODO.md \
	mruby/Doxyfile \
	mruby/mrblib/00kernel.rb \
	mruby/mrblib/range.rb \
	mruby/mrblib/kernel.rb \
	mruby/mrblib/hash.rb \
	mruby/mrblib/00class.rb \
	mruby/mrblib/10error.rb \
	mruby/mrblib/array.rb \
	mruby/mrblib/string.rb \
	mruby/mrblib/compar.rb \
	mruby/mrblib/symbol.rb \
	mruby/mrblib/enum.rb \
	mruby/mrblib/numeric.rb \
	mruby/build_config.rb \
	mruby/LICENSE \
	mruby/src/etc.c \
	mruby/src/variable.c \
	mruby/src/cdump.c \
	mruby/src/init.c \
	mruby/src/opcode.h \
	mruby/src/gc.c \
	mruby/src/load.c \
	mruby/src/codedump.c \
	mruby/src/error.c \
	mruby/src/readfloat.c \
	mruby/src/state.c \
	mruby/src/string.c \
	mruby/src/numeric.c \
	mruby/src/readnum.c \
	mruby/src/enum.c \
	mruby/src/print.c \
	mruby/src/readint.c \
	mruby/src/numops.c \
	mruby/src/hash.c \
	mruby/src/version.c \
	mruby/src/backtrace.c \
	mruby/src/fmt_fp.c \
	mruby/src/range.c \
	mruby/src/vm.c \
	mruby/src/debug.c \
	mruby/src/symbol.c \
	mruby/src/error.h \
	mruby/src/kernel.c \
	mruby/src/object.c \
	mruby/src/proc.c \
	mruby/src/array.c \
	mruby/src/dump.c \
	mruby/src/class.c \
	mruby/src/value_array.h \
	mruby/src/allocf.c \
	mruby/src/pool.c \
	mruby/mruby-source.gemspec \
	mruby/mrbgems/mruby-eval/mrbgem.rake \
	mruby/mrbgems/mruby-eval/test/binding.rb \
	mruby/mrbgems/mruby-eval/test/eval.rb \
	mruby/mrbgems/mruby-eval/src/eval.c \
	mruby/mrbgems/stdlib.gembox \
	mruby/mrbgems/mruby-string-ext/mrbgem.rake \
	mruby/mrbgems/mruby-string-ext/test/range.rb \
	mruby/mrbgems/mruby-string-ext/test/string.rb \
	mruby/mrbgems/mruby-string-ext/test/numeric.rb \
	mruby/mrbgems/mruby-string-ext/mrblib/string.rb \
	mruby/mrbgems/mruby-string-ext/src/string.c \
	mruby/mrbgems/default-no-stdio.gembox \
	mruby/mrbgems/mruby-set/mrbgem.rake \
	mruby/mrbgems/mruby-set/README.md \
	mruby/mrbgems/mruby-set/test/set.rb \
	mruby/mrbgems/mruby-set/mrblib/set.rb \
	mruby/mrbgems/mruby-set/LICENSE \
	mruby/mrbgems/mruby-set/mruby-set.gem \
	mruby/mrbgems/mruby-method/mrbgem.rake \
	mruby/mrbgems/mruby-method/README.md \
	mruby/mrbgems/mruby-method/test/method.rb \
	mruby/mrbgems/mruby-method/mrblib/method.rb \
	mruby/mrbgems/mruby-method/src/method.c \
	mruby/mrbgems/mruby-random/mrbgem.rake \
	mruby/mrbgems/mruby-random/test/random.rb \
	mruby/mrbgems/mruby-random/src/random.c \
	mruby/mrbgems/mruby-catch/mrbgem.rake \
	mruby/mrbgems/mruby-catch/test/catch.rb \
	mruby/mrbgems/mruby-catch/mrblib/catch.rb \
	mruby/mrbgems/mruby-catch/src/catch.c \
	mruby/mrbgems/mruby-cmath/mrbgem.rake \
	mruby/mrbgems/mruby-cmath/test/cmath.rb \
	mruby/mrbgems/mruby-cmath/src/cmath.c \
	mruby/mrbgems/mruby-compar-ext/mrbgem.rake \
	mruby/mrbgems/mruby-compar-ext/mrblib/compar.rb \
	mruby/mrbgems/mruby-pack/mrbgem.rake \
	mruby/mrbgems/mruby-pack/README.md \
	mruby/mrbgems/mruby-pack/test/pack.rb \
	mruby/mrbgems/mruby-pack/src/pack.c \
	mruby/mrbgems/mruby-struct/mrbgem.rake \
	mruby/mrbgems/mruby-struct/test/struct.rb \
	mruby/mrbgems/mruby-struct/mrblib/struct.rb \
	mruby/mrbgems/mruby-struct/src/struct.c \
	mruby/mrbgems/mruby-bigint/core/bigint.c \
	mruby/mrbgems/mruby-bigint/core/bigint.h \
	mruby/mrbgems/mruby-bigint/mrbgem.rake \
	mruby/mrbgems/mruby-bigint/README.md \
	mruby/mrbgems/mruby-bigint/test/bigint.rb \
	mruby/mrbgems/mruby-bigint/README-fgmp.md \
	mruby/mrbgems/mruby-symbol-ext/mrbgem.rake \
	mruby/mrbgems/mruby-symbol-ext/test/symbol.rb \
	mruby/mrbgems/mruby-symbol-ext/mrblib/symbol.rb \
	mruby/mrbgems/mruby-symbol-ext/src/symbol.c \
	mruby/mrbgems/mruby-io/mrbgem.rake \
	mruby/mrbgems/mruby-io/README.md \
	mruby/mrbgems/mruby-io/test/mruby_io_test.c \
	mruby/mrbgems/mruby-io/test/io.rb \
	mruby/mrbgems/mruby-io/test/file.rb \
	mruby/mrbgems/mruby-io/test/file_test.rb \
	mruby/mrbgems/mruby-io/mrblib/io.rb \
	mruby/mrbgems/mruby-io/mrblib/file.rb \
	mruby/mrbgems/mruby-io/mrblib/file_constants.rb \
	mruby/mrbgems/mruby-io/mrblib/kernel.rb \
	mruby/mrbgems/mruby-io/src/mruby_io_gem.c \
	mruby/mrbgems/mruby-io/src/file_test.c \
	mruby/mrbgems/mruby-io/src/io.c \
	mruby/mrbgems/mruby-io/src/file.c \
	mruby/mrbgems/mruby-io/include/mruby/ext/io.h \
	mruby/mrbgems/mruby-compiler/core/parse.y \
	mruby/mrbgems/mruby-compiler/core/y.tab.c \
	mruby/mrbgems/mruby-compiler/core/node.h \
	mruby/mrbgems/mruby-compiler/core/keywords \
	mruby/mrbgems/mruby-compiler/core/codegen.c \
	mruby/mrbgems/mruby-compiler/core/lex.def \
	mruby/mrbgems/mruby-compiler/mrbgem.rake \
	mruby/mrbgems/mruby-bin-config/mrbgem.rake \
	mruby/mrbgems/mruby-bin-config/mruby-config \
	mruby/mrbgems/mruby-bin-config/mruby-config.bat \
	mruby/mrbgems/mruby-proc-ext/mrbgem.rake \
	mruby/mrbgems/mruby-proc-ext/test/proc.rb \
	mruby/mrbgems/mruby-proc-ext/test/proc.c \
	mruby/mrbgems/mruby-proc-ext/mrblib/proc.rb \
	mruby/mrbgems/mruby-proc-ext/src/proc.c \
	mruby/mrbgems/mruby-data/mrbgem.rake \
	mruby/mrbgems/mruby-data/test/data.rb \
	mruby/mrbgems/mruby-data/src/data.c \
	mruby/mrbgems/mruby-dir/mrbgem.rake \
	mruby/mrbgems/mruby-dir/README.md \
	mruby/mrbgems/mruby-dir/test/dir.rb \
	mruby/mrbgems/mruby-dir/test/dirtest.c \
	mruby/mrbgems/mruby-dir/mrblib/dir.rb \
	mruby/mrbgems/mruby-dir/src/Win/dirent.c \
	mruby/mrbgems/mruby-dir/src/dir.c \
	mruby/mrbgems/mruby-object-ext/mrbgem.rake \
	mruby/mrbgems/mruby-object-ext/test/nil.rb \
	mruby/mrbgems/mruby-object-ext/test/object.rb \
	mruby/mrbgems/mruby-object-ext/mrblib/object.rb \
	mruby/mrbgems/mruby-object-ext/src/object.c \
	mruby/mrbgems/mruby-kernel-ext/mrbgem.rake \
	mruby/mrbgems/mruby-kernel-ext/test/kernel.rb \
	mruby/mrbgems/mruby-kernel-ext/src/kernel.c \
	mruby/mrbgems/mruby-class-ext/mrbgem.rake \
	mruby/mrbgems/mruby-class-ext/test/class.rb \
	mruby/mrbgems/mruby-class-ext/test/module.rb \
	mruby/mrbgems/mruby-class-ext/mrblib/module.rb \
	mruby/mrbgems/mruby-class-ext/src/class.c \
	mruby/mrbgems/mruby-binding/mrbgem.rake \
	mruby/mrbgems/mruby-binding/test/binding.rb \
	mruby/mrbgems/mruby-binding/test/binding.c \
	mruby/mrbgems/mruby-binding/src/binding.c \
	mruby/mrbgems/mruby-print/mrbgem.rake \
	mruby/mrbgems/mruby-print/mrblib/print.rb \
	mruby/mrbgems/mruby-print/src/print.c \
	mruby/mrbgems/mruby-hash-ext/mrbgem.rake \
	mruby/mrbgems/mruby-hash-ext/test/hash.rb \
	mruby/mrbgems/mruby-hash-ext/mrblib/hash.rb \
	mruby/mrbgems/mruby-hash-ext/src/hash-ext.c \
	mruby/mrbgems/mruby-sleep/example/sleep.rb \
	mruby/mrbgems/mruby-sleep/mrbgem.rake \
	mruby/mrbgems/mruby-sleep/README.md \
	mruby/mrbgems/mruby-sleep/test/sleep_test.rb \
	mruby/mrbgems/mruby-sleep/src/sleep.c \
	mruby/mrbgems/mruby-sprintf/mrbgem.rake \
	mruby/mrbgems/mruby-sprintf/test/sprintf.rb \
	mruby/mrbgems/mruby-sprintf/mrblib/string.rb \
	mruby/mrbgems/mruby-sprintf/src/sprintf.c \
	mruby/mrbgems/mruby-enum-chain/mrbgem.rake \
	mruby/mrbgems/mruby-enum-chain/test/enum_chain.rb \
	mruby/mrbgems/mruby-enum-chain/mrblib/chain.rb \
	mruby/mrbgems/mruby-range-ext/mrbgem.rake \
	mruby/mrbgems/mruby-range-ext/test/range.rb \
	mruby/mrbgems/mruby-range-ext/mrblib/range.rb \
	mruby/mrbgems/mruby-range-ext/src/range.c \
	mruby/mrbgems/mruby-bin-mirb/tools/mirb/mirb.c \
	mruby/mrbgems/mruby-bin-mirb/mrbgem.rake \
	mruby/mrbgems/mruby-bin-mirb/bintest/mirb.rb \
	mruby/mrbgems/mruby-metaprog/mrbgem.rake \
	mruby/mrbgems/mruby-metaprog/test/metaprog.rb \
	mruby/mrbgems/mruby-metaprog/src/metaprog.c \
	mruby/mrbgems/mruby-test-inline-struct/mrbgem.rake \
	mruby/mrbgems/mruby-test-inline-struct/test/inline.c \
	mruby/mrbgems/mruby-test-inline-struct/test/inline.rb \
	mruby/mrbgems/mruby-time/mrbgem.rake \
	mruby/mrbgems/mruby-time/test/time.rb \
	mruby/mrbgems/mruby-time/src/time.c \
	mruby/mrbgems/mruby-time/include/mruby/time.h \
	mruby/mrbgems/mruby-proc-binding/mrbgem.rake \
	mruby/mrbgems/mruby-proc-binding/test/proc-binding.c \
	mruby/mrbgems/mruby-proc-binding/test/proc-binding.rb \
	mruby/mrbgems/mruby-proc-binding/src/proc-binding.c \
	mruby/mrbgems/stdlib-ext.gembox \
	mruby/mrbgems/metaprog.gembox \
	mruby/mrbgems/mruby-enumerator/mrbgem.rake \
	mruby/mrbgems/mruby-enumerator/test/enumerator.rb \
	mruby/mrbgems/mruby-enumerator/mrblib/enumerator.rb \
	mruby/mrbgems/math.gembox \
	mruby/mrbgems/mruby-socket/mrbgem.rake \
	mruby/mrbgems/mruby-socket/README.md \
	mruby/mrbgems/mruby-socket/test/socket.rb \
	mruby/mrbgems/mruby-socket/test/udpsocket.rb \
	mruby/mrbgems/mruby-socket/test/tcpsocket.rb \
	mruby/mrbgems/mruby-socket/test/sockettest.c \
	mruby/mrbgems/mruby-socket/test/basicsocket.rb \
	mruby/mrbgems/mruby-socket/test/addrinfo.rb \
	mruby/mrbgems/mruby-socket/test/unix.rb \
	mruby/mrbgems/mruby-socket/test/ipsocket.rb \
	mruby/mrbgems/mruby-socket/mrblib/socket.rb \
	mruby/mrbgems/mruby-socket/src/socket.c \
	mruby/mrbgems/mruby-socket/src/gen.rb \
	mruby/mrbgems/mruby-socket/src/const.cstub \
	mruby/mrbgems/mruby-socket/src/const.def \
	mruby/mrbgems/mruby-objectspace/mrbgem.rake \
	mruby/mrbgems/mruby-objectspace/test/objectspace.rb \
	mruby/mrbgems/mruby-objectspace/src/mruby_objectspace.c \
	mruby/mrbgems/full-core.gembox \
	mruby/mrbgems/mruby-rational/mrbgem.rake \
	mruby/mrbgems/mruby-rational/test/rational.rb \
	mruby/mrbgems/mruby-rational/mrblib/rational.rb \
	mruby/mrbgems/mruby-rational/src/rational.c \
	mruby/mrbgems/mruby-numeric-ext/mrbgem.rake \
	mruby/mrbgems/mruby-numeric-ext/test/numeric.rb \
	mruby/mrbgems/mruby-numeric-ext/mrblib/numeric_ext.rb \
	mruby/mrbgems/mruby-numeric-ext/src/numeric_ext.c \
	mruby/mrbgems/mruby-fiber/mrbgem.rake \
	mruby/mrbgems/mruby-fiber/test/fiber2.rb \
	mruby/mrbgems/mruby-fiber/test/fibertest.c \
	mruby/mrbgems/mruby-fiber/test/fiber.rb \
	mruby/mrbgems/mruby-fiber/src/fiber.c \
	mruby/mrbgems/mruby-complex/mrbgem.rake \
	mruby/mrbgems/mruby-complex/test/complex.rb \
	mruby/mrbgems/mruby-complex/mrblib/complex.rb \
	mruby/mrbgems/mruby-complex/src/complex.c \
	mruby/mrbgems/mruby-exit/mrbgem.rake \
	mruby/mrbgems/mruby-exit/src/mruby-exit.c \
	mruby/mrbgems/mruby-error/mrbgem.rake \
	mruby/mrbgems/mruby-error/test/exception.rb \
	mruby/mrbgems/mruby-error/test/exception.c \
	mruby/mrbgems/mruby-error/src/exception.c \
	mruby/mrbgems/mruby-bin-mruby/tools/mruby/mruby.c \
	mruby/mrbgems/mruby-bin-mruby/mrbgem.rake \
	mruby/mrbgems/mruby-bin-mruby/bintest/mruby.rb \
	mruby/mrbgems/mruby-test/mrbgem.rake \
	mruby/mrbgems/mruby-test/vformat.c \
	mruby/mrbgems/mruby-test/README.md \
	mruby/mrbgems/mruby-test/driver.c \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/cmdbreak.c \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/apiprint.c \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/cmdrun.c \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/apistring.h \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/apiprint.h \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/apibreak.h \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/apibreak.c \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/mrdberror.h \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/apistring.c \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/apilist.h \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/cmdmisc.c \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/cmdprint.c \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/mrdbconf.h \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/mrdb.h \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/mrdb.c \
	mruby/mrbgems/mruby-bin-debugger/tools/mrdb/apilist.c \
	mruby/mrbgems/mruby-bin-debugger/mrbgem.rake \
	mruby/mrbgems/mruby-bin-debugger/bintest/mrdb.rb \
	mruby/mrbgems/mruby-bin-debugger/bintest/print.rb \
	mruby/mrbgems/mruby-array-ext/mrbgem.rake \
	mruby/mrbgems/mruby-array-ext/test/array.rb \
	mruby/mrbgems/mruby-array-ext/mrblib/array.rb \
	mruby/mrbgems/mruby-array-ext/src/array.c \
	mruby/mrbgems/mruby-enum-lazy/mrbgem.rake \
	mruby/mrbgems/mruby-enum-lazy/test/lazy.rb \
	mruby/mrbgems/mruby-enum-lazy/mrblib/lazy.rb \
	mruby/mrbgems/mruby-bin-strip/tools/mruby-strip/mruby-strip.c \
	mruby/mrbgems/mruby-bin-strip/mrbgem.rake \
	mruby/mrbgems/mruby-bin-strip/bintest/mruby-strip.rb \
	mruby/mrbgems/mruby-errno/mrbgem.rake \
	mruby/mrbgems/mruby-errno/README.md \
	mruby/mrbgems/mruby-errno/test/errno.rb \
	mruby/mrbgems/mruby-errno/mrblib/errno.rb \
	mruby/mrbgems/mruby-errno/src/gen.rb \
	mruby/mrbgems/mruby-errno/src/known_errors_def.cstub \
	mruby/mrbgems/mruby-errno/src/known_errors.def \
	mruby/mrbgems/mruby-errno/src/errno.c \
	mruby/mrbgems/mruby-toplevel-ext/mrbgem.rake \
	mruby/mrbgems/mruby-toplevel-ext/test/toplevel.rb \
	mruby/mrbgems/mruby-toplevel-ext/mrblib/toplevel.rb \
	mruby/mrbgems/mruby-math/mrbgem.rake \
	mruby/mrbgems/mruby-math/test/math.rb \
	mruby/mrbgems/mruby-math/src/math.c \
	mruby/mrbgems/mruby-bin-mrbc/tools/mrbc/stub.c \
	mruby/mrbgems/mruby-bin-mrbc/tools/mrbc/mrbc.c \
	mruby/mrbgems/mruby-bin-mrbc/mrbgem.rake \
	mruby/mrbgems/mruby-bin-mrbc/bintest/mrbc.rb \
	mruby/mrbgems/stdlib-io.gembox \
	mruby/mrbgems/default.gembox \
	mruby/mrbgems/mruby-os-memsize/mrbgem.rake \
	mruby/mrbgems/mruby-os-memsize/test/memsize.rb \
	mruby/mrbgems/mruby-os-memsize/src/memsize.c \
	mruby/mrbgems/default-no-fpu.gembox \
	mruby/mrbgems/mruby-enum-ext/mrbgem.rake \
	mruby/mrbgems/mruby-enum-ext/test/enum.rb \
	mruby/mrbgems/mruby-enum-ext/mrblib/enum.rb \
	mruby/Rakefile \
	mruby/SECURITY.md \
	mruby/doc/mruby_logo_red_icon.png \
	mruby/doc/mruby3.0.md \
	mruby/doc/mruby3.1.md \
	mruby/doc/internal/opcode.md \
	mruby/doc/internal/boxing.md \
	mruby/doc/limitations.md \
	mruby/doc/mruby3.2.md \
	mruby/doc/mruby3.3.md \
	mruby/doc/guides/compile.md \
	mruby/doc/guides/memory.md \
	mruby/doc/guides/mrbgems.md \
	mruby/doc/guides/debugger.md \
	mruby/doc/guides/link.md \
	mruby/doc/guides/symbol.md \
	mruby/doc/guides/gc-arena-howto.md \
	mruby/doc/guides/mrbconf.md \
	mruby/minirake \
	mruby/include/mruby/opcode.h \
	mruby/include/mruby/re.h \
	mruby/include/mruby/hash.h \
	mruby/include/mruby/string.h \
	mruby/include/mruby/presym.h \
	mruby/include/mruby/object.h \
	mruby/include/mruby/class.h \
	mruby/include/mruby/ops.h \
	mruby/include/mruby/irep.h \
	mruby/include/mruby/compile.h \
	mruby/include/mruby/array.h \
	mruby/include/mruby/range.h \
	mruby/include/mruby/throw.h \
	mruby/include/mruby/variable.h \
	mruby/include/mruby/boxing_word.h \
	mruby/include/mruby/istruct.h \
	mruby/include/mruby/data.h \
	mruby/include/mruby/common.h \
	mruby/include/mruby/error.h \
	mruby/include/mruby/debug.h \
	mruby/include/mruby/boxing_no.h \
	mruby/include/mruby/numeric.h \
	mruby/include/mruby/boxing_nan.h \
	mruby/include/mruby/value.h \
	mruby/include/mruby/endian.h \
	mruby/include/mruby/internal.h \
	mruby/include/mruby/dump.h \
	mruby/include/mruby/version.h \
	mruby/include/mruby/khash.h \
	mruby/include/mruby/gc.h \
	mruby/include/mruby/presym/enable.h \
	mruby/include/mruby/presym/disable.h \
	mruby/include/mruby/presym/scanning.h \
	mruby/include/mruby/proc.h \
	mruby/include/mrbconf.h \
	mruby/include/mruby.h

if ENABLE_THIRD_PARTY

noinst_LTLIBRARIES = liburlparse.la
liburlparse_la_SOURCES = \
	urlparse/urlparse.c \
	urlparse/urlparse.h

noinst_LTLIBRARIES += libllhttp.la
libllhttp_la_SOURCES = \
	llhttp/src/api.c \
	llhttp/src/http.c \
	llhttp/src/llhttp.c \
	llhttp/include/llhttp.h
libllhttp_la_CPPFLAGS = -I${srcdir}/llhttp/include

if HAVE_NEVERBLEED
noinst_LTLIBRARIES += libneverbleed.la
libneverbleed_la_CPPFLAGS = @OPENSSL_CFLAGS@ -D_GNU_SOURCE
libneverbleed_la_LIBADD = @OPENSSL_LIBS@
libneverbleed_la_SOURCES = neverbleed/neverbleed.c neverbleed/neverbleed.h
endif # HAVE_NEVERBLEED

if HAVE_MRUBY

.PHONY: all-local clean mruby

mruby:
	mkdir -p "${abs_builddir}/mruby/build"
	diff "${srcdir}/build_config.rb" "${abs_builddir}/mruby/build/build_config.rb" >& /dev/null || \
	cp "${srcdir}/build_config.rb" "${abs_builddir}/mruby/build"
	MRUBY_CONFIG="${abs_builddir}/mruby/build/build_config.rb" \
	BUILD_DIR="${abs_builddir}/mruby/build" \
	INSTALL_DIR="${abs_builddir}/mruby/build/install/bin" \
	MRUBY_CC="${CC}" MRUBY_CXX="$(firstword $(CXX))" MRUBY_LD="${LD}" \
	MRUBY_AR="${AR}" \
	HOST="${host}" BUILD="${build}" \
	"${srcdir}/mruby/minirake" -f "${srcdir}/mruby/Rakefile"

all-local: mruby

clean-local:
	[ ! -f "${abs_builddir}/mruby/build/build_config.rb" ] || \
	MRUBY_CONFIG="${abs_builddir}/mruby/build/build_config.rb" \
	BUILD_DIR="${abs_builddir}/mruby/build" \
	MRUBY_CC="${CC}" \
	"${srcdir}/mruby/minirake" -f "${srcdir}/mruby/Rakefile" clean

endif # HAVE_MRUBY

endif # ENABLE_THIRD_PARTY
