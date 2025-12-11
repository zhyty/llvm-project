# TODO(toyang): what does this do?
USE_LIBDL := 1

# Build the main executable - handled by default rules below
# Build the shared library from sources in lib/ subdirectory
a.out: lib_helper

# Include standard rules for the main executable build
# This will build main/main.cpp and main/utils.cpp into nested .o files
CXX_SOURCES := main.cpp utils.cpp

# We don't need to link against libhelper (e.g. -L. -llibhelper) since we
# explicitly load it in the test program via dlopen.

include Makefile.rules

# Build shared library using the standard LLDB pattern
lib_helper: lib
	mkdir -p lib
	"$(MAKE)" VPATH=$(SRCDIR)/lib -C lib -f $(MAKEFILE_RULES) \
		DYLIB_ONLY=YES DYLIB_CXX_SOURCES="helper.cpp utils.cpp" DYLIB_NAME=helper
