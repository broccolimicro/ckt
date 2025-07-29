NAME          = lm
DEPEND        = interpret_sch interpret_prs interpret_hse interpret_chp weaver chp interpret_flow interpret_arithmetic interpret_boolean hse prs sch interpret_phy flow phy petri arithmetic boolean parse_verilog parse_prs parse_cog parse_chp parse_astg parse_spice parse_dot parse_expression parse_ucs parse common

SRCDIR        = src
TESTDIR       = tests
GTEST        := ../../googletest
GTEST_I      := -I$(GTEST)/googletest/include -I.
GTEST_L      := -L$(GTEST)/build/lib -L.

INCLUDE_PATHS = $(DEPEND:%=-I../../lib/%) -I../../lib/gdstk/build/include $(shell python3-config --includes) -I.
LIBRARY_PATHS = $(DEPEND:%=-L../../lib/%) -L.
LIBRARIES     = $(DEPEND:%=-l%) -ldl
LIBFILES      = $(foreach dep,$(DEPEND),../../lib/$(dep)/lib$(dep).a)
CXXFLAGS      = -std=c++20 -g -Wall -fmessage-length=0 -D CL_HPP_MINIMUM_OPENCL_VERSION=120 -D CL_HPP_TARGET_OPENCL_VERSION=120 -D CL_HPP_ENABLE_EXCEPTIONS 
LDFLAGS       = 

SOURCES	     := $(shell mkdir -p $(SRCDIR); find $(SRCDIR) -name '*.cpp')
OBJECTS	     := $(SOURCES:%.cpp=build/%.o)
DEPS         := $(shell mkdir -p build/$(SRCDIR); find build/$(SRCDIR) -name '*.d')
TARGET	      = $(NAME)

TESTS        := $(shell mkdir -p tests; find $(TESTDIR) -name '*.cpp')
TEST_OBJECTS := $(TESTS:%.cpp=build/%.o) build/$(TESTDIR)/gtest_main.o
TEST_DEPS    := $(shell mkdir -p build/$(TESTDIR); find build/$(TESTDIR) -name '*.d')
TEST_TARGET   = test

COVERAGE ?= 0

ifeq ($(COVERAGE),0)
CXXFLAGS += -O2
else
CXXFLAGS += -O0 --coverage -fprofile-arcs -ftest-coverage
LDFLAGS  += --coverage -fprofile-arcs -ftest-coverage 
endif

ifndef VERSION
override VERSION = "develop"
endif

ifeq ($(OS),Windows_NT)
	CXXFLAGS += -D WIN32
	ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
		CXXFLAGS += -D AMD64
	else
		ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
			CXXFLAGS += -D AMD64
		endif
		ifeq ($(PROCESSOR_ARCHITECTURE),x86)
			CXXFLAGS += -D IA32
		endif
	endif
	LIBRARIES += -l:libgdstk.a -l:libclipper.a -l:libqhullstatic_r.a -lz -lOpenCL
	LIBRARY_PATHS += -L../../lib/gdstk/build/lib -L../../lib/gdstk/build/lib64
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		CXXFLAGS += -D LINUX
		LIBRARIES += -l:libgdstk.a -l:libclipper.a -l:libqhullstatic_r.a -lz -lOpenCL
		LIBRARY_PATHS += -L../../lib/gdstk/build/lib -L../../lib/gdstk/build/lib64
	endif
	ifeq ($(UNAME_S),Darwin)
		CXXFLAGS += -D OSX -mmacos-version-min=15.0 -Wno-missing-braces
		INCLUDE_PATHS += -I$(shell brew --prefix qhull)/include -I$(shell brew --prefix graphviz)/include -I$(shell brew --prefix opencl-headers)/include -I$(shell brew --prefix opencl-clhpp-headers)/include
		LIBRARY_PATHS += -L$(shell brew --prefix qhull)/lib -L$(shell brew --prefix graphviz)/lib
		LIBRARIES += -lgdstk -lclipper -lqhullstatic_r -lz -framework OpenCL
		LIBRARY_PATHS += -L../../lib/gdstk/build/lib
		LDFLAGS	      += -Wl,-rpath,/opt/homebrew/opt/python@3.15/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.14/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.13/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.12/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.11/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.10/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.09/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python/Frameworks/Python.framework/Versions/Current/lib
	endif
	UNAME_P := $(shell uname -p)
	ifeq ($(UNAME_P),x86_64)
		CXXFLAGS += -D AMD64
	endif
	ifneq ($(filter %86,$(UNAME_P)),)
		CXXFLAGS += -D IA32
	endif
	ifneq ($(filter arm%,$(UNAME_P)),)
		CXXFLAGS += -D ARM
	endif
endif

all: version setgv $(TARGET)

nogv: $(TARGET)

setgv:
	$(eval CXXFLAGS += -DGRAPHVIZ_SUPPORTED=1)
	$(eval LIBRARIES += -lcgraph -lgvc)

version:
	echo "const char *version = \"${VERSION}\";" > src/version.h

$(TARGET): $(OBJECTS) $(LIBFILES)
	$(CXX) $(LIBRARY_PATHS) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) -o $(TARGET) $(LIBRARIES)

build/$(SRCDIR)/%.o: $(SRCDIR)/%.cpp 
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDE_PATHS) -MM -MF $(patsubst %.o,%.d,$@) -MT $@ -c $<
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATHS) -c -o $@ $<

$(TEST_TARGET): setgv $(TEST_OBJECTS) $(filter-out build/$(SRCDIR)/main.o, $(OBJECTS))
	$(CXX) $(LIBRARY_PATHS) $(CXXFLAGS) $(GTEST_L) $(INCLUDE_PATHS) $(filter-out $(firstword $^), $^) -o $(TEST_TARGET) -pthread -lgtest $(LIBRARIES)

build/$(TESTDIR)/%.o: $(TESTDIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(GTEST_I) $(INCLUDE_PATHS) -MM -MF $(patsubst %.o,%.d,$@) -MT $@ -c $<
	$(CXX) $(CXXFLAGS) $(GTEST_I) $(INCLUDE_PATHS) $< -c -o $@

build/$(TESTDIR)/gtest_main.o: $(GTEST)/googletest/src/gtest_main.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(GTEST_I) $< -c -o $@

include $(DEPS) $(TEST_DEPS)

clean:
	rm -rf build $(TARGET) $(TEST_TARGET)
