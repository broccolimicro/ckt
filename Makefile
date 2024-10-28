PYTHON_RELEASE = python$(shell python3 -c "import sys;sys.stdout.write('{}.{}'.format(sys.version_info[0],sys.version_info[1]))")

NAME          = lm
DEPEND        = interpret_sch interpret_prs interpret_hse interpret_chp interpret_arithmetic interpret_boolean interpret_ucs chp hse prs sch interpret_phy phy petri arithmetic boolean ucs parse_prs parse_chp parse_astg parse_spice parse_dot parse_expression parse_ucs parse common

SRCDIR        = src
TESTDIR       = tests
GTEST        := ../../googletest
GTEST_I      := -I$(GTEST)/googletest/include -I.
GTEST_L      := -L$(GTEST)/build/lib -L.

INCLUDE_PATHS = $(DEPEND:%=-I../../lib/%) -I../../lib/gdstk/build/include $(shell python3-config --includes) -I.
LIBRARY_PATHS = $(DEPEND:%=-L../../lib/%) -L$(shell python3-config --prefix)/lib -L.
LIBRARIES     = $(DEPEND:%=-l%) -l$(PYTHON_RELEASE)
LIBFILES      = $(foreach dep,$(DEPEND),../../lib/$(dep)/lib$(dep).a)
CXXFLAGS      = -std=c++17 -O2 -g -Wall -fmessage-length=0
LDFLAGS	      =  

SOURCES	     := $(shell mkdir -p $(SRCDIR); find $(SRCDIR) -name '*.cpp')
OBJECTS	     := $(SOURCES:%.cpp=build/%.o)
DEPS         := $(shell mkdir -p build/$(SRCDIR); find build/$(SRCDIR) -name '*.d')
TARGET	      = $(NAME)

TESTS        := $(shell mkdir -p tests; find $(TESTDIR) -name '*.cpp')
TEST_OBJECTS := $(TESTS:%.cpp=build/%.o) build/$(TESTDIR)/gtest_main.o
TEST_DEPS    := $(shell mkdir -p build/$(TESTDIR); find build/$(TESTDIR) -name '*.d')
TEST_TARGET   = test

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
	LIBRARIES += -l:libgdstk.a -l:libclipper.a -l:libqhullstatic_r.a -lz
	LIBRARY_PATHS += -L../../lib/gdstk/build/lib -L../../lib/gdstk/build/lib64
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		CXXFLAGS += -D LINUX
		LIBRARIES += -l:libgdstk.a -l:libclipper.a -l:libqhullstatic_r.a -lz
		LIBRARY_PATHS += -L../../lib/gdstk/build/lib -L../../lib/gdstk/build/lib64
	endif
	ifeq ($(UNAME_S),Darwin)
		CXXFLAGS += -D OSX -mmacos-version-min=12.0 -Wno-missing-braces
		INCLUDE_PATHS += -I$(shell brew --prefix qhull)/include -I$(shell brew --prefix graphviz)/include
		LIBRARY_PATHS += -L$(shell brew --prefix qhull)/lib -L$(shell brew --prefix graphviz)/lib
		LIBRARIES += -lgdstk -lclipper -lqhullstatic_r -lz
		LIBRARY_PATHS += -L../../lib/gdstk/build/lib
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
