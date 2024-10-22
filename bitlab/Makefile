# Compiler
CC = gcc

# Compiler flags
CFLAGS = -std=c11 -Wall -Wextra

# Include directories
INCLUDES = -Iinclude

# Libraries
CLIBS = 

# Source files
CSRCS = $(wildcard src/*.c)

# Object files
COBJS = $(CSRCS:.c=.o)
COBJS := $(addprefix build/, $(COBJS))

# Executable name
MAIN = main

.PHONY: default all debug clean depend

default: all

all: $(MAIN)
	@echo "Bitlab has been compiled"

debug: CFLAGS += -g -DDEBUG -D_DEBUG
debug: $(MAIN)
	@echo "Bitlab has been compiled in debug mode"

### Choose one of the following for C or C++ compilation -------------------

$(MAIN): $(COBJS)
	@mkdir -p build/bin
	$(CC) $(CFLAGS) $(INCLUDES) -o build/bin/$(MAIN) $(COBJS) $(CLIBS)

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

depend: $(CSRCS)
	makedepend $(INCLUDES) $^

### -----------------------------------------------------------------------

clean:
	$(RM) build/src/*.o *~ $(MAIN)
	$(RM) build/bin/$(MAIN)
	$(RM) build/lib/*.a

# Include dependency files
-include $(SRCS:.c=.d)
