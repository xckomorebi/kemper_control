CFLAGS := -I./include -O2 -Wall -Wextra -Wpedantic
LDFLAGS := -lncurses -lportmidi

CFILES := $(wildcard src/*.c)
OBJFILES := $(patsubst src/%.c, output/%.o, $(CFILES))

TARGET := kemper_control
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

DEBUG ?= 0

ifeq ($(DEBUG), 1)
	CFLAGS += -g
endif

.PHONY: all
all: build

install: $(TARGET)
	install $(TARGET) $(BINDIR)

uninstall:
	rm -f $(BINDIR)/$(TARGET)

.PHONY: build
build: $(TARGET)

.PHONY: run
run: $(TARGET)
	./$(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) -o $@ $^ $(LDFLAGS)

output/%.o: src/%.c
	@mkdir -p output
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(TARGET)
	rm -f output/*.o
	rm -rf output
	rm -rf *.dSYM
