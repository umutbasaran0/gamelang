CC      = gcc
CFLAGS  = -std=gnu99 -Wall -Wextra -Wpedantic -g
TARGET  = gamelang
SRCDIR  = src

SRCS    = $(SRCDIR)/main.c $(SRCDIR)/lexer.c $(SRCDIR)/ast.c \
          $(SRCDIR)/parser.c $(SRCDIR)/typecheck.c \
          $(SRCDIR)/interpreter.c $(SRCDIR)/environment.c
OBJS    = $(SRCS:.c=.o)

ifeq ($(OS),Windows_NT)
    RM  = del /Q /F
    EXT = .exe
else
    RM  = rm -f
    EXT =
endif

all: $(TARGET)$(EXT)

$(TARGET)$(EXT): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean test

clean:
	$(RM) $(SRCDIR)/*.o
	$(RM) $(TARGET)$(EXT)

test:
	@echo "=== Running valid tests ==="
	@pass=0; fail=0; \
	for f in tests/valid/*.game; do \
	  if ./$(TARGET) "$$f" > /dev/null 2>&1; then \
	    echo "  PASS  $$f"; pass=$$((pass+1)); \
	  else \
	    echo "  FAIL  $$f"; fail=$$((fail+1)); \
	  fi; \
	done; \
	echo "=== Running invalid tests (expect errors) ==="; \
	for f in tests/invalid/*.game; do \
	  if ./$(TARGET) "$$f" 2>&1 | grep -q "Error"; then \
	    echo "  PASS  $$f (error caught)"; pass=$$((pass+1)); \
	  else \
	    echo "  FAIL  $$f (should have errored)"; fail=$$((fail+1)); \
	  fi; \
	done; \
	echo ""; echo "Results: $$pass passed, $$fail failed"