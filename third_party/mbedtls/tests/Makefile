
# To compile on SunOS: add "-lsocket -lnsl" to LDFLAGS

CFLAGS	?= -O2
WARNING_CFLAGS ?= -Wall -Wextra -Wformat=2 -Wno-format-nonliteral
LDFLAGS ?=

# Set this to -v to see the details of failing test cases
TEST_FLAGS ?= $(if $(filter-out 0 OFF Off off NO No no FALSE False false N n,$(CTEST_OUTPUT_ON_FAILURE)),-v,)

default: all

# Include public header files from ../include, test-specific header files
# from ./include, and private header files (used by some invasive tests)
# from ../library.
LOCAL_CFLAGS = $(WARNING_CFLAGS) -I./include -I../include -I../library -D_FILE_OFFSET_BITS=64
LOCAL_LDFLAGS = -L../library			\
		-lmbedtls$(SHARED_SUFFIX)	\
		-lmbedx509$(SHARED_SUFFIX)	\
		-lmbedcrypto$(SHARED_SUFFIX)

include ../3rdparty/Makefile.inc
LOCAL_CFLAGS+=$(THIRDPARTY_INCLUDES)

# Enable definition of various functions used throughout the testsuite
# (gethostname, strdup, fileno...) even when compiling with -std=c99. Harmless
# on non-POSIX platforms.
LOCAL_CFLAGS += -D_POSIX_C_SOURCE=200809L

ifndef SHARED
MBEDLIBS=../library/libmbedcrypto.a ../library/libmbedx509.a ../library/libmbedtls.a
else
MBEDLIBS=../library/libmbedcrypto.$(DLEXT) ../library/libmbedx509.$(DLEXT) ../library/libmbedtls.$(DLEXT)
endif

ifdef DEBUG
LOCAL_CFLAGS += -g3
endif

ifdef RECORD_PSA_STATUS_COVERAGE_LOG
LOCAL_CFLAGS += -Werror -DRECORD_PSA_STATUS_COVERAGE_LOG
endif

# if we're running on Windows, build for Windows
ifdef WINDOWS
WINDOWS_BUILD=1
endif

ifdef WINDOWS_BUILD
DLEXT=dll
EXEXT=.exe
LOCAL_LDFLAGS += -lws2_32
ifdef SHARED
SHARED_SUFFIX=.$(DLEXT)
endif
else
DLEXT ?= so
EXEXT=
SHARED_SUFFIX=
endif

ifdef WINDOWS
PYTHON ?= python
else
PYTHON ?= $(shell if type python3 >/dev/null 2>/dev/null; then echo python3; else echo python; fi)
endif

.PHONY: generated_files
GENERATED_DATA_FILES := $(patsubst tests/%,%,$(shell \
	$(PYTHON) scripts/generate_psa_tests.py --list || \
	echo FAILED \
))
ifeq ($(GENERATED_DATA_FILES),FAILED)
$(error "$(PYTHON) scripts/generate_psa_tests.py --list" failed)
endif
GENERATED_FILES := $(GENERATED_DATA_FILES)
generated_files: $(GENERATED_FILES)

# generate_psa_tests.py spends more time analyzing inputs than generating
# outputs. Its inputs are the same no matter which files are being generated.
# It's rare not to want all the outputs. So always generate all of its outputs.
# Use an intermediate phony dependency so that parallel builds don't run
# a separate instance of the recipe for each output file.
.SECONDARY: generated_psa_test_data
$(GENERATED_DATA_FILES): generated_psa_test_data
generated_psa_test_data: scripts/generate_psa_tests.py
generated_psa_test_data: ../scripts/mbedtls_dev/crypto_knowledge.py
generated_psa_test_data: ../scripts/mbedtls_dev/macro_collector.py
generated_psa_test_data: ../scripts/mbedtls_dev/psa_storage.py
generated_psa_test_data: ../scripts/mbedtls_dev/test_case.py
## The generated file only depends on the options that are present in
## crypto_config.h, not on which options are set. To avoid regenerating this
## file all the time when switching between configurations, don't declare
## crypto_config.h as a dependency. Remove this file from your working tree
## if you've just added or removed an option in crypto_config.h.
#generated_psa_test_data: ../include/psa/crypto_config.h
generated_psa_test_data: ../include/psa/crypto_values.h
generated_psa_test_data: ../include/psa/crypto_extra.h
generated_psa_test_data: suites/test_suite_psa_crypto_metadata.data
generated_psa_test_data:
	echo "  Gen   $(GENERATED_DATA_FILES) ..."
	$(PYTHON) scripts/generate_psa_tests.py

# A test application is built for each suites/test_suite_*.data file.
# Application name is same as .data file's base name and can be
# constructed by stripping path 'suites/' and extension .data.
DATA_FILES := $(wildcard suites/test_suite_*.data)
# Make sure that generated data files are included even if they don't
# exist yet when the makefile is parsed.
DATA_FILES += $(filter-out $(DATA_FILES),$(GENERATED_DATA_FILES))
APPS = $(basename $(subst suites/,,$(DATA_FILES)))

# Construct executable name by adding OS specific suffix $(EXEXT).
BINARIES := $(addsuffix $(EXEXT),$(APPS))

.SILENT:

.PHONY: all check test clean

all: $(BINARIES)

$(MBEDLIBS):
	$(MAKE) -C ../library

MBEDTLS_TEST_OBJS=$(patsubst %.c,%.o,$(wildcard src/*.c src/drivers/*.c))

mbedtls_test: $(MBEDTLS_TEST_OBJS)

TEST_OBJS_DEPS = $(wildcard include/test/*.h include/test/*/*.h)
ifdef RECORD_PSA_STATUS_COVERAGE_LOG
# Explicitly depend on this header because on a clean copy of the source tree,
# it doesn't exist yet and must be generated as part of the build, and
# therefore the wildcard enumeration above doesn't include it.
TEST_OBJS_DEPS += include/test/instrument_record_status.h
endif

# Rule to compile common test C files in src folder
src/%.o : src/%.c $(TEST_OBJS_DEPS)
	echo "  CC    $<"
	$(CC) $(LOCAL_CFLAGS) $(CFLAGS) -o $@ -c $<

src/drivers/%.o : src/drivers/%.c
	echo "  CC    $<"
	$(CC) $(LOCAL_CFLAGS) $(CFLAGS) -o $@ -c $<

C_FILES := $(addsuffix .c,$(APPS))

# Wildcard target for test code generation:
# A .c file is generated for each .data file in the suites/ directory. Each .c
# file depends on a .data and .function file from suites/ directory. Following
# nameing convention is followed:
#
#     C file        |        Depends on
#-----------------------------------------------------------------------------
#  foo.c            | suites/foo.function suites/foo.data
#  foo.bar.c        | suites/foo.function suites/foo.bar.data
#
# Note above that .c and .data files have same base name.
# However, corresponding .function file's base name is the word before first
# dot in .c file's base name.
#
.SECONDEXPANSION:
%.c: suites/$$(firstword $$(subst ., ,$$*)).function suites/%.data scripts/generate_test_code.py suites/helpers.function suites/main_test.function suites/host_test.function
	echo "  Gen   $@"
	$(PYTHON) scripts/generate_test_code.py -f suites/$(firstword $(subst ., ,$*)).function \
		-d suites/$*.data \
		-t suites/main_test.function \
		-p suites/host_test.function \
		-s suites  \
		--helpers-file suites/helpers.function \
		-o .


$(BINARIES): %$(EXEXT): %.c $(MBEDLIBS) $(TEST_OBJS_DEPS) $(MBEDTLS_TEST_OBJS)
	echo "  CC    $<"
	$(CC) $(LOCAL_CFLAGS) $(CFLAGS) $< $(MBEDTLS_TEST_OBJS) $(LOCAL_LDFLAGS) $(LDFLAGS) -o $@

clean:
ifndef WINDOWS
	rm -rf $(BINARIES) *.c *.datax
	rm -f src/*.o src/drivers/*.o src/libmbed*
	rm -f include/test/instrument_record_status.h
	rm -rf libtestdriver1
else
	if exist *.c del /Q /F *.c
	if exist *.exe del /Q /F *.exe
	if exist *.datax del /Q /F *.datax
	if exist src/*.o del /Q /F src/*.o
	if exist src/drivers/*.o del /Q /F src/drivers/*.o
	if exist src/libmbed* del /Q /F src/libmed*
	if exist include/test/instrument_record_status.h del /Q /F include/test/instrument_record_status.h
endif

neat: clean
ifndef WINDOWS
	rm -f $(GENERATED_FILES)
else
	for %f in ($(subst /,\,$(GENERATED_FILES))) if exist %f del /Q /F %f
endif

# Test suites caught by SKIP_TEST_SUITES are built but not executed.
check: $(BINARIES)
	perl scripts/run-test-suites.pl $(TEST_FLAGS) --skip=$(SKIP_TEST_SUITES)

test: check

# Generate test library

# Perl code that is executed to transform each original line from a library
# source file into the corresponding line in the test driver copy of the
# library. Add a LIBTESTDRIVER1_/libtestdriver1_ to mbedtls_xxx and psa_xxx
# symbols.
define libtestdriver1_rewrite :=
	s!^(\s*#\s*include\s*[\"<])(mbedtls|psa)/!$${1}libtestdriver1/include/$${2}/!; \
	next if /^\s*#\s*include/; \
	s/\b(?=MBEDTLS_|PSA_)/LIBTESTDRIVER1_/g; \
	s/\b(?=mbedtls_|psa_)/libtestdriver1_/g;
endef

libtestdriver1.a:
	# Copy the library and fake a 3rdparty Makefile include.
	rm -Rf ./libtestdriver1
	mkdir ./libtestdriver1
	cp -Rf ../library ./libtestdriver1
	cp -Rf ../include ./libtestdriver1
	cp -Rf ../scripts ./libtestdriver1
	mkdir ./libtestdriver1/3rdparty
	touch ./libtestdriver1/3rdparty/Makefile.inc

	# Set the test driver base (minimal) configuration.
	cp ./include/test/drivers/config_test_driver.h ./libtestdriver1/include/mbedtls/mbedtls_config.h

	# Set the PSA cryptography configuration for the test library.
	# It is set from the copied include/psa/crypto_config.h of the Mbed TLS
        # library the test library is intended to be linked with extended by
        # ./include/test/drivers/crypto_config_test_driver_extension.h to
        # mirror the PSA_ACCEL_* macros.
	mv ./libtestdriver1/include/psa/crypto_config.h ./libtestdriver1/include/psa/crypto_config.h.bak
	head -n -1 ./libtestdriver1/include/psa/crypto_config.h.bak > ./libtestdriver1/include/psa/crypto_config.h
	cat ./include/test/drivers/crypto_config_test_driver_extension.h >> ./libtestdriver1/include/psa/crypto_config.h
	echo "#endif /* PSA_CRYPTO_CONFIG_H */" >> ./libtestdriver1/include/psa/crypto_config.h

	# Prefix MBEDTLS_* PSA_* symbols with LIBTESTDRIVER1_ as well as
	# mbedtls_* psa_* symbols with libtestdriver1_ to avoid symbol clash
	# when this test driver library is linked with the Mbed TLS library.
	perl -pi -e '$(libtestdriver1_rewrite)' ./libtestdriver1/library/*.[ch]
	perl -pi -e '$(libtestdriver1_rewrite)' ./libtestdriver1/include/*/*.h

	$(MAKE) -C ./libtestdriver1/library CFLAGS="-I../../ $(CFLAGS)" LDFLAGS="$(LDFLAGS)" libmbedcrypto.a
	cp ./libtestdriver1/library/libmbedcrypto.a ../library/libtestdriver1.a

ifdef RECORD_PSA_STATUS_COVERAGE_LOG
include/test/instrument_record_status.h: ../include/psa/crypto.h Makefile
	echo "  Gen  $@"
	sed <../include/psa/crypto.h >$@ -n 's/^psa_status_t \([A-Za-z0-9_]*\)(.*/#define \1(...) RECORD_STATUS("\1", \1(__VA_ARGS__))/p'
endif
