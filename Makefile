# JumiTerm - ANSI BBS terminal for the Neo6502
# Local overrides (gitignored): set NEOCC / EMU here if they aren't on PATH
-include config.mk

NEOCC   ?= mos-neo6502-clang
EMU     ?= neo
HOSTCC  ?= gcc
PYTHON  ?= python3

NEOFLAGS  = -Os -Wall -Isrc
HOSTFLAGS = -std=c99 -Wall -Wextra -Werror -g -Isrc -Itests

CORE   = $(wildcard src/*.c)
NEOSRC = $(wildcard src/neo/*.c)
HDRS   = $(wildcard src/*.h src/neo/*.h tests/*.h)
STUBS  = tests/render_stub.c
TESTS  = $(patsubst tests/%.c,build/tests/%,$(wildcard tests/test_*.c))

.PHONY: all test ctest pytest font bench run clean

all: build/jumiterm.neo

build/jumiterm.neo: $(CORE) $(NEOSRC) $(HDRS)
	@mkdir -p build
	$(NEOCC) $(NEOFLAGS) -o $@ $(CORE) $(NEOSRC)

bench: build/bench.neo

build/bench.neo: spike/bench.c src/neo/api.c src/neo/render_neo.c src/font_data.c $(HDRS)
	@mkdir -p build
	$(NEOCC) $(NEOFLAGS) -o $@ spike/bench.c src/neo/api.c src/neo/render_neo.c src/font_data.c

test: ctest pytest

ctest: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; $$t || exit 1; done

build/tests/%: tests/%.c $(CORE) $(STUBS) $(HDRS)
	@mkdir -p build/tests
	$(HOSTCC) $(HOSTFLAGS) -o $@ $< $(CORE) $(STUBS)

pytest: build/xfer_host
	$(PYTHON) -m unittest discover -s tools -p 'test_*.py'

XFER_SRC = tools/xfer_host.c src/xmodem.c src/fname.c src/util.c

build/xfer_host: $(XFER_SRC) src/xmodem.h src/fname.h src/util.h
	@mkdir -p build
	$(HOSTCC) $(HOSTFLAGS) -o $@ $(XFER_SRC)

font:
	$(PYTHON) tools/mkfont.py src/font_data.c build/font_preview.png

run: build/jumiterm.neo
	@mkdir -p build/storage
	cd build && $(EMU) jumiterm.neo keys

clean:
	rm -rf build
