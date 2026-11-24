CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic
TARGET = openbor-smart-c
SOURCES = main.c lexer.c parser.c expression.c converter.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) test-c

test-c: test.c lexer.c parser.c expression.c converter.c
	$(CC) $(CFLAGS) -o $@ test.c lexer.c parser.c expression.c converter.c

test: test-c
	./test-c

.PHONY: all clean test
