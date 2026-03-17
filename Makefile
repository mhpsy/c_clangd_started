CC      = clang
CFLAGS  = -Wall -Wextra -std=c11 -g
SRC     = src/main.c
TARGET  = hello

.PHONY: all clean run compile_commands

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

# generate compile_commands.json for clangd
compile_commands:
	bear -- make
