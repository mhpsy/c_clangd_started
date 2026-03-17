CC      = clang
CFLAGS  = -Wall -Wextra -std=c11 -g
SRC     = $(wildcard src/*.c)
TARGET  = build/ch347_run_st7789
LDLIBS  = -lusb-1.0

.PHONY: all clean run compile_commands

all: $(TARGET)

build:
	mkdir -p $@

$(TARGET): $(SRC) Makefile | build
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

# generate compile_commands.json for clangd
compile_commands:
	bear -- make
