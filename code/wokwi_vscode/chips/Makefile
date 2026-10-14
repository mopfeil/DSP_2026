# Compile the Wokwi custom chips to WebAssembly for Wokwi for VS Code
# (the browser simulator compiles chip.c itself; VS Code needs the .wasm).
#
#   make WASI_SDK=/path/to/wasi-sdk     (https://github.com/WebAssembly/wasi-sdk/releases)
#
# Sources: the generated single-file chips in ../../wokwi/chips/
# (run tools/make_chips.sh first after changing engine_signals.h).
# wokwi-api.h is downloaded from wokwi.com on the first build.
WASI_SDK ?= $(HOME)/wasi-sdk
CC       = $(WASI_SDK)/bin/clang
CFLAGS   = --target=wasm32-wasip1 --sysroot=$(WASI_SDK)/share/wasi-sysroot -O2 -I.
LDFLAGS  = -nostartfiles -Wl,--import-memory -Wl,--export-table -Wl,--no-entry
SRC      = ../../wokwi/chips
CHIPS    = engine-sim lambda-probe

all: $(CHIPS:%=%.chip.wasm) $(CHIPS:%=%.chip.json)

wokwi-api.h:
	curl -fsSL -o $@ https://wokwi.com/api/chips/wokwi-api.h

%.chip.wasm: $(SRC)/%.chip.c wokwi-api.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -lm
	md5sum $< | cut -d' ' -f1 > $*.chip.src.md5

engine-sim.chip.json: $(SRC)/engine-sim.chip.json
	cp $< $@

lambda-probe.chip.json: $(SRC)/lambda-probe.chip.json
	cp $< $@

clean:
	rm -f *.chip.wasm *.chip.src.md5

.PHONY: all clean
