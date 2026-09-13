CC = gcc
FLEX = flex

CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc

TARGET = scanner

SRC = \
	src/main.c \
	src/token.c \
	src/statistics.c \
	src/token_list.c \
	src/source.c \
	src/preprocessor.c \
	src/presentation.c

SCANNER = build/scanner.c

OBJ = \
	build/main.o \
	build/token.o \
	build/statistics.o \
	build/token_list.o \
	build/source.o \
	build/preprocessor.o \
	build/presentation.o \
	build/scanner.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) -lm

build/scanner.c: src/scanner.l
	mkdir -p build
	$(FLEX) -o $@ $<

build/%.o: src/%.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/main.o: src/main.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/scanner.o: build/scanner.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET)
	rm -f build/*.o
	rm -f build/scanner.c
	rm -f output/presentation.aux
	rm -f output/presentation.log
	rm -f output/presentation.nav
	rm -f output/presentation.out
	rm -f output/presentation.snm
	rm -f output/presentation.toc
	rm -f output/presentation.vrb
	rm -f output/presentation.pdf
	rm -f output/presentation.tex

.PHONY: all clean