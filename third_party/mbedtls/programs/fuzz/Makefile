MBEDTLS_TEST_PATH:=../../tests/src
MBEDTLS_TEST_OBJS:=$(patsubst %.c,%.o,$(wildcard ${MBEDTLS_TEST_PATH}/*.c ${MBEDTLS_TEST_PATH}/drivers/*.c))

LOCAL_CFLAGS = -I../../tests/include -I../../include -D_FILE_OFFSET_BITS=64
LOCAL_LDFLAGS = ${MBEDTLS_TEST_OBJS}		\
		-L../../library			\
		-lmbedtls$(SHARED_SUFFIX)	\
		-lmbedx509$(SHARED_SUFFIX)	\
		-lmbedcrypto$(SHARED_SUFFIX)

LOCAL_CFLAGS += $(patsubst -I../%,-I../../%,$(THIRDPARTY_INCLUDES))

ifndef SHARED
DEP=../../library/libmbedcrypto.a ../../library/libmbedx509.a ../../library/libmbedtls.a
else
DEP=../../library/libmbedcrypto.$(DLEXT) ../../library/libmbedx509.$(DLEXT) ../../library/libmbedtls.$(DLEXT)
endif


DLEXT ?= so
EXEXT=
SHARED_SUFFIX=

ifdef FUZZINGENGINE
LOCAL_LDFLAGS += -lFuzzingEngine
endif

# A test application is built for each suites/test_suite_*.data file.
# Application name is same as .data file's base name and can be
# constructed by stripping path 'suites/' and extension .data.
APPS = $(basename $(wildcard fuzz_*.c))

# Construct executable name by adding OS specific suffix $(EXEXT).
BINARIES := $(addsuffix $(EXEXT),$(APPS))

.SILENT:

.PHONY: all check test clean

all: $(BINARIES)

$(DEP):
	$(MAKE) -C ../../library

C_FILES := $(addsuffix .c,$(APPS))

%.o: %.c
	$(CC) $(LOCAL_CFLAGS) $(CFLAGS) -c $<	-o $@


ifdef FUZZINGENGINE
$(BINARIES): %$(EXEXT): %.o common.o $(DEP)
	echo " $(CC) common.o $< $(LOCAL_LDFLAGS) $(LDFLAGS) -o $@"
	$(CXX) common.o $<	$(LOCAL_LDFLAGS) $(LDFLAGS) -o $@
else
$(BINARIES): %$(EXEXT): %.o common.o onefile.o $(DEP)
	echo " $(CC) common.o onefile.o $< $(LOCAL_LDFLAGS) $(LDFLAGS) -o $@"
	$(CC) common.o onefile.o $<	$(LOCAL_LDFLAGS) $(LDFLAGS) -o $@
endif

clean:
ifndef WINDOWS
	rm -rf $(BINARIES) *.o
else
	if exist *.o del /Q /F *.o
	if exist *.exe del /Q /F *.exe
endif
