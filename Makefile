CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/field.c src/svg.c src/render.c src/gif.c
EMCC    ?= emcc
PORT    ?= 4190

.PHONY: all test bench story story-check web smoke play plots clean
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

LINE   := src/field.c src/line.c
LINE_H := src/field.h src/line.h tests/line_policy.h tests/line_run.h

test_line: $(LINE) tests/test_line.c $(LINE_H)
	$(CC) $(CFLAGS) -o $@ $(LINE) tests/test_line.c $(LDLIBS)

story_bench: $(LINE) tests/story_bench.c $(LINE_H)
	$(CC) $(CFLAGS) -o $@ $(LINE) tests/story_bench.c $(LDLIBS)

# The story rows are compared byte for byte; generate and check them with the
# Debian 13 toolchain of the Dockerfile so libm rounds the same way everywhere.
test: test_field test_gif test_observer test_line story_bench
	./test_field
	./test_gif
	./test_observer
	./test_line
	./story_bench --check docs/narrative

# Rewrites docs/narrative/{play-order,ablation,tension}.json.
story: story_bench
	mkdir -p docs/narrative
	./story_bench docs/narrative

# Regenerates every row into a scratch directory and compares the three files byte for byte.
story-check: story_bench
	rm -rf .story-check && mkdir .story-check
	./story_bench .story-check
	cmp .story-check/play-order.json docs/narrative/play-order.json
	cmp .story-check/ablation.json docs/narrative/ablation.json
	cmp .story-check/tension.json docs/narrative/tension.json
	rm -rf .story-check

bench: bench_field
	./bench_field

web: dist/field.mjs
	cp web/index.html web/style.css web/app.mjs dist/
	mkdir -p dist/assets
	cp assets/observatory.jpg dist/assets/
	python3 scripts/check-size.py

dist/field.mjs: src/field.c src/observer.c src/line.c src/line_web.c src/field.h src/observer.h src/line.h
	mkdir -p dist
	$(EMCC) $(CFLAGS) src/field.c src/observer.c src/line.c src/line_web.c -lm --no-entry -o $@ \
	  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node \
	  -sEXPORTED_FUNCTIONS=_fr_reset,_fr_step,_fr_time,_fr_position,_fr_velocity,_fr_energy,_fr_width,_fr_stopped,_fr_samples,_fr_count,_lw_reset,_lw_tick,_lw_answer,_lw_value,_lw_samples,_lw_count \
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
	rm -f field-ripple test_field test_gif test_observer bench_field test_line story_bench
	rm -rf dist
