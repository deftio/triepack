# terseml -- C implementation.
#
# This subproject is self-contained: it does not link triepack and does not
# need its build. `make check` is the whole contract -- the shared vector
# corpus, the grammar's rejection cases, and a C++ link of the same object.
#
# Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
# BSD-2-Clause -- see LICENSE.txt

CC      ?= cc
CXX     ?= c++
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra -pedantic
LDFLAGS ?=

SAN := -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all check test sanitize cxx bench vectors clean

all: tsml

tsml: tsml_cli.c terseml.c terseml.h
	$(CC) $(CFLAGS) -o $@ tsml_cli.c terseml.c $(LDFLAGS)

test_terseml_c: test_terseml.c terseml.c terseml.h vectors.h
	$(CC) $(CFLAGS) -o $@ test_terseml.c terseml.c $(LDFLAGS)

test_terseml_cxx: test_terseml_cxx.cpp terseml.c terseml.h
	$(CC) $(CFLAGS) -c terseml.c -o terseml.o
	$(CXX) -std=c++11 -Wall -Wextra -o $@ test_terseml_cxx.cpp terseml.o

# The header claims to be usable from C++. It is only true if someone builds
# it that way, so that is a test and not a comment.
cxx: test_terseml_cxx
	./test_terseml_cxx

test: test_terseml_c
	./test_terseml_c

# The C decoder hands out pointers into the caller's buffer and allocates from
# an arena. Both are exactly the shapes a sanitizer exists to check.
sanitize: test_terseml.c terseml.c terseml.h vectors.h
	$(CC) -O1 -g -std=c99 -Wall -Wextra $(SAN) -o test_terseml_asan \
	    test_terseml.c terseml.c
	./test_terseml_asan

check: test sanitize cxx
	@command -v python3 >/dev/null && python3 -m pytest -q test_terseml.py test_xml_bridge.py || \
	    echo "skipped: python3 not found"
	@command -v node >/dev/null && node test_terseml.js || echo "skipped: node not found"

# vectors.h and vectors.json come from one generator, so the three
# implementations cannot drift apart by testing different corpora.
vectors: make_vectors.py terseml.py
	python3 make_vectors.py > vectors.json

bench: tsml
	@test -n "$(FILE)" || (echo "usage: make bench FILE=some.tsml"; exit 1)
	./tsml bench $(or $(N),10) < $(FILE)

clean:
	rm -f tsml test_terseml_c test_terseml_cxx test_terseml_asan terseml.o
	rm -rf *.dSYM
