SWIG_FILES := $(wildcard *.i)
SWIG        = swig

PY_TESTS   := $(wildcard tests/*.py)
TESTS      := $(wildcard build/lib*)

reshop_wrap.c: $(SWIG_FILES) ../src/api/reshop.h
	RESHOP_SRCDIR=$$(pwd)/../src SWIG_FEATURES="-I$$(pwd)/../src/api" ./pybuild.sh

$(TESTS): reshop_wrap.c $(PY_TESTS)
	cd tests; PYTHONPATH=$$(pwd)/../$(TESTS) LD_LIBRARY_PATH=$$(pwd)/../reshop/ pytest

test: $(TESTS)

all: reshop_wrap.c

.PHONY: test $(TESTS)
