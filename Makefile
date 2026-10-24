CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/field.c src/svg.c src/render.c src/gif.c
EMCC    ?= emcc
PORT    ?= 4190

.PHONY: all test bench web smoke play plots clean
all: field-ripple

field-ripple: $(SRC) src/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_field: $(SRC) tests/test_field.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_gif: src/gif.c tests/test_gif.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_observer: src/field.c src/observer.c tests/test_observer.c src/field.h src/observer.h
	$(CC) $(CFLAGS) -o $@ src/field.c src/observer.c tests/test_observer.c $(LDLIBS)

bench_field: src/field.c src/observer.c bench/bench.c src/field.h src/observer.h
	$(CC) $(CFLAGS) -o $@ src/field.c src/observer.c bench/bench.c $(LDLIBS)

test: test_field test_gif test_observer
	./test_field
	./test_gif
	./test_observer

bench: bench_field
	./bench_field

web: dist/field.mjs
	cp web/index.html web/style.css web/app.mjs dist/
	mkdir -p dist/assets
	cp assets/observatory.jpg dist/assets/
	python3 scripts/check-size.py

dist/field.mjs: src/field.c src/observer.c src/field.h src/observer.h
	mkdir -p dist
	$(EMCC) $(CFLAGS) src/field.c src/observer.c -lm --no-entry -o $@ \
	  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node \
	  -sEXPORTED_FUNCTIONS=_fr_reset,_fr_step,_fr_time,_fr_position,_fr_velocity,_fr_energy,_fr_width,_fr_stopped,_fr_samples,_fr_count \
	  -sEXPORTED_RUNTIME_METHODS=HEAPF64

smoke: web
	node scripts/smoke.mjs

play: web
	python3 -m http.server $(PORT) --bind 127.0.0.1 --directory dist

# Regenerates the CSV and the SVG charts shown in the README.
plots: field-ripple
	mkdir -p docs
	./field-ripple --headless docs
	./field-ripple --gif docs/demo.gif
	head -c6 docs/demo.gif | grep -q GIF89a

clean:
	rm -f field-ripple test_field test_gif test_observer bench_field
	rm -rf dist
