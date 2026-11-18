CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
# The terminal mode of src/main.c is POSIX; the feature macro is set here because a name starting
# with an underscore and a capital letter is reserved inside the program.
POSIX   := -D_POSIX_C_SOURCE=200809L
SRC     := src/field.c src/svg.c src/render.c src/gif.c src/pair.c
EMCC    ?= emcc
PORT    ?= 4190

.PHONY: all test bench story story-check web smoke play plots clean theory-figs theory-check
all: field-ripple

field-ripple: $(SRC) src/main.c src/pair.h
	$(CC) $(CFLAGS) $(POSIX) -o $@ $(SRC) src/main.c $(LDLIBS)

test_field: $(SRC) tests/test_field.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_field.c $(LDLIBS)

test_gif: src/gif.c tests/test_gif.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_observer: src/field.c src/observer.c tests/test_observer.c src/field.h src/observer.h
	$(CC) $(CFLAGS) -o $@ src/field.c src/observer.c tests/test_observer.c $(LDLIBS)

bench_field: src/field.c src/observer.c bench/bench.c src/field.h src/observer.h
	$(CC) $(CFLAGS) -o $@ src/field.c src/observer.c bench/bench.c $(LDLIBS)

PAIR   := src/field.c src/pair.c src/pair_web.c

test_pair: $(PAIR) tests/test_pair.c src/field.h src/pair.h
	$(CC) $(CFLAGS) -o $@ $(PAIR) tests/test_pair.c $(LDLIBS)

LINE   := src/field.c src/line.c
LINE_H := src/field.h src/line.h tests/line_policy.h tests/line_run.h

test_line: $(LINE) tests/test_line.c $(LINE_H)
	$(CC) $(CFLAGS) -o $@ $(LINE) tests/test_line.c $(LDLIBS)

story_bench: $(LINE) tests/story_bench.c $(LINE_H)
	$(CC) $(CFLAGS) -o $@ $(LINE) tests/story_bench.c $(LDLIBS)

# The story rows are compared byte for byte; generate and check them with the
# Debian 13 toolchain of the Dockerfile so libm rounds the same way everywhere.
test: test_field test_gif test_observer test_line story_bench test_pair
	./test_field
	./test_gif
	./test_observer
	./test_line
	./test_pair
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
	cp web/index.html web/style.css web/app.mjs web/collide.mjs dist/
	mkdir -p dist/assets
	cp assets/observatory.jpg dist/assets/
	python3 scripts/check_size.py

dist/field.mjs: src/field.c src/observer.c src/line.c src/line_web.c src/pair.c src/pair_web.c src/field.h src/observer.h src/line.h src/pair.h
	mkdir -p dist
	$(EMCC) $(CFLAGS) src/field.c src/observer.c src/line.c src/line_web.c src/pair.c src/pair_web.c -lm --no-entry -o $@ \
	  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node \
	  -sEXPORTED_FUNCTIONS=_fr_reset,_fr_step,_fr_time,_fr_position,_fr_velocity,_fr_energy,_fr_width,_fr_stopped,_fr_samples,_fr_count,_lw_reset,_lw_command,_lw_tick,_lw_answer,_lw_value,_lw_samples,_lw_count,_pw_reset,_pw_step,_pw_value,_pw_samples,_pw_history,_pw_count,_pw_columns,_pw_rows \
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
	./field-ripple --collide docs
	head -c6 docs/demo.gif | grep -q GIF89a

# Regenerates the plotted figures and the measured-values table of docs/theory (one command).
theory_figs: $(LINE) src/pair.c src/svg.c tests/theory_figs.c $(LINE_H) src/svg.h src/pair.h
	$(CC) $(CFLAGS) -o $@ $(LINE) src/pair.c src/svg.c tests/theory_figs.c $(LDLIBS)

theory-figs: theory_figs
	mkdir -p docs/theory/figures
	./theory_figs docs/theory/figures

# Fails when a statement of docs/theory has no figure, a link is broken, or a referenced function or test is gone.
theory-check:
	python3 scripts/theory_check.py

clean:
	rm -f field-ripple test_field test_gif test_observer bench_field test_line story_bench theory_figs test_pair
	rm -rf dist

# ---- M61: local-only test standard (~/project-context/rules/test-standard.json) ----
# `make test` above stays the CI gate. `make check` runs every adopted kind through the
# common isolated runner (tm test) and writes git-ignored JUnit + test-results/summary.json.
TM_TEST_PYTHON ?= python3.14
TM_TEST_CMAKE ?= cmake
TM_TEST_CTEST ?= ctest
TM_TEST_NPM_CACHE ?= $(HOME)/.npm
TM_UNIT_BINS := test_field test_gif test_observer test_line test_pair story_bench

.PHONY: test-unit check test-trace test-perf test-e2e tm-ctest tm-deps test-tm-unit test-tm-trace test-tm-perf test-tm-e2e
test-unit:
	$(TM_TEST_PYTHON) scripts/local_test.py unit

check:
	$(TM_TEST_PYTHON) scripts/local_test.py all

test-trace test-perf test-e2e:
	$(TM_TEST_PYTHON) scripts/local_test.py $(@:test-%=%)

tm-ctest:
	mkdir -p test-results
	"$(TM_TEST_CMAKE)" -S tests/ctest -B .tm-ctest >/dev/null

test-tm-unit test-tm-trace: tm-ctest
	$(MAKE) -j1 $(TM_UNIT_BINS)
	"$(TM_TEST_CTEST)" --test-dir .tm-ctest -j 1 -L '^$(@:test-tm-%=%)$$' --output-on-failure --output-junit "$(CURDIR)/test-results/$(@:test-tm-%=%).xml"

test-tm-perf: tm-ctest
	"$(TM_TEST_CTEST)" --test-dir .tm-ctest -j 1 -L '^perf$$' --output-on-failure --output-junit "$(CURDIR)/test-results/perf.xml"

tm-deps:
	@test -n "$(TM_TEST_RUN_ID)" || { echo "Requires isolated tm test" >&2; exit 1; }
	mkdir -p .tm-home && touch .tm-home/user.npmrc .tm-home/global.npmrc
	@test -d node_modules/playwright || npm ci --offline --ignore-scripts --no-audit --no-fund --userconfig="$(CURDIR)/.tm-home/user.npmrc" --globalconfig="$(CURDIR)/.tm-home/global.npmrc" --cache="$(TM_TEST_NPM_CACHE)"

test-tm-e2e: tm-ctest tm-deps
	"$(TM_TEST_CTEST)" --test-dir .tm-ctest -j 1 -L '^e2e$$' --output-on-failure --output-junit "$(CURDIR)/test-results/e2e.xml"

# ---- Formatter, linter and type checker (no ignores) ----
# C: clang-format, clang-tidy (.clang-tidy) and the compiler with -Wall -Wextra -Wpedantic -Werror.
# Python (scripts/*.py): Ruff format, Ruff with every rule, mypy strict. These are pinned by uv.lock.
# JavaScript, CSS, HTML, JSON: Biome (recommended rules, warnings fail) and tsc --checkJs strict,
# pinned by package-lock.json. TIDY_EXTRA passes -isystem paths when the C library headers are elsewhere.
RUN        = uv run --locked
STYLE_C    = $(shell git ls-files '*.c' '*.h')
STYLE_TU   = $(shell git ls-files '*.c')
TIDY_EXTRA ?=
.PHONY: fmt lint typecheck check-style
fmt:
	$(RUN) clang-format -i $(STYLE_C)
	$(RUN) ruff format scripts
	$(RUN) ruff check --fix scripts
	npm run fmt
lint:
	$(RUN) clang-format --dry-run --Werror $(STYLE_C)
	$(RUN) clang-tidy $(STYLE_TU) -- -std=c11 -Isrc -Itests $(POSIX) $(TIDY_EXTRA)
	$(RUN) ruff format --check scripts
	$(RUN) ruff check scripts
	npm run lint
typecheck:
	for f in $(STYLE_TU); do $(CC) $(CFLAGS) $(POSIX) -Isrc -Itests -fsyntax-only $$f || exit 1; done
	$(RUN) mypy
	npm run typecheck
check-style: lint typecheck
