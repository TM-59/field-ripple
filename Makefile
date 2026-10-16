CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/field.c src/svg.c src/render.c

.PHONY: all test plots clean
all: field-ripple

field-ripple: $(SRC) src/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_field: $(SRC) tests/test_field.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test: test_field
	./test_field

# Regenerates the CSV and the SVG charts shown in the README.
plots: field-ripple
	mkdir -p docs
	./field-ripple --headless docs

clean:
	rm -f field-ripple test_field
